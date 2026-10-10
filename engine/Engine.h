#ifndef ENGINE_H
#define ENGINE_H

#include <string>
#include <vector>
#include "Types.h"
#include "Graph.h"
#include "Dijkstra.h"
#include "Queue.h"
#include "RequestList.h"
#include "DeliveryLog.h"
#include "AVLTree.h"
#include "Sorting.h"

enum class Err { None, Invalid, Duplicate, NotFound, ConfirmNeeded, Empty, Unreachable, OutOfStock };

struct OpResult {
    Err         err;       // None means success
    std::string message;   // a short human-readable sentence, set for success AND failure
    int         value;     // extra result: see each function. 0 if the function has none.
    bool ok() const { return err == Err::None; }
};

struct Stock { int food; int water; int medicine; };

struct RankedRequest {     // one row of the ranked active list
    Request  req;
    bool     hasReason;    // false for the last row
    RankKey  reason;       // first key where req differs from the NEXT row; meaningful only if hasReason
};

struct RouteInfo {         // the route from the depot to one camp, from the latest Dijkstra run
    int              campId;
    bool             reachable;
    int              minutes;    // UNREACHABLE if not reachable
    std::vector<int> path;       // depot to camp, empty if not reachable
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void reset();   // clears every structure. Afterwards: no camps, graph holds only node 0,
                    // depot at (0, 0), speed 40 km/h, stock 500 food / 500 water / 500 medicine,
                    // both counters back to 1, trace "Reset."

    // ---- camps ----
    OpResult addCamp(const Camp& c, const std::vector<int>& linkTo);
    OpResult editCamp(int id, const std::string& name, double x, double y, int families);
    OpResult deleteCamp(int id, bool confirm);
    bool     findCamp(int id, Camp& out) const;          // AVL search
    std::vector<Camp> camps() const;                     // ascending id
    int      campCount() const;
    OpResult setDepotPosition(double x, double y);

    // ---- roads ----
    OpResult addRoad(int a, int b);                      // length comes from the two positions
    OpResult removeRoad(int a, int b);
    OpResult setRoadStatus(int a, int b, RoadStatus s);
    OpResult setAverageSpeed(double kmh);

    // ---- requests and delivery ----
    OpResult submitRequest(int campId, ResourceType r, int quantity, int urgency, int families);
    OpResult processIncoming();
    OpResult updateRequest(int requestId, int urgency, int families, int quantity);
    OpResult handleNext();
    OpResult setStock(int food, int water, int medicine);

    // ---- scenario (defined in Scenario.cpp) ----
    OpResult loadScenario(const std::string& text);

    // ---- read-only views, for the console and the future server ----
    std::vector<Request>        queueContents() const;       // front to back
    std::vector<RankedRequest>  rankedRequests() const;      // active list, best first
    std::vector<DeliveryRecord> deliveries() const;          // oldest first
    std::vector<RouteInfo>      routes() const;              // one per camp, ascending id
    std::vector<EdgeInfo>       roads() const;               // Graph::edgeList
    void   avlSnapshot(std::vector<AVLNodeInfo>& out) const;
    const std::vector<RotationEvent>& lastRotations() const; // from the AVL tree
    const Dijkstra& dijkstra() const;
    Stock  stock() const;
    double averageSpeed() const;
    double depotX() const;
    double depotY() const;
    int    pendingCountForCamp(int campId) const;            // requests in the queue plus the active list
    const std::string& lastTrace() const;                    // the operation trace, see 6.3

private:
    AVLTree       camps_;
    Graph         graph_;
    Dijkstra      dijkstra_;
    Queue         queue_;
    RequestList   activeRequests_;
    DeliveryLog   log_;

    double        depotX_;
    double        depotY_;
    Stock         stock_;
    int           nextRequestId_;
    int           nextArrivalSeq_;
    std::string   lastTrace_;

    // Helpers
    bool positionOf(int id, double& outX, double& outY) const;
    double distanceBetween(int a, int b) const;
    void runDijkstra();
};

#endif
