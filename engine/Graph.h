#ifndef GRAPH_H
#define GRAPH_H

#include <vector>

enum class RoadStatus { Open, Slow, Blocked };

struct EdgeInfo {
    int        a;
    int        b;
    double     distanceKm;
    RoadStatus status;
    int        minutes;
};

struct Neighbor {
    int id;
    int minutes;
};

class Graph {
public:
    // Initializes an empty graph. O(1)
    Graph();
    // Frees all nodes. O(V + E)
    ~Graph();
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    // Adds a node in ascending id order. false if id < 0 or exists. O(V)
    bool addNode(int id);
    
    // Removes the node AND every road touching it. false if not found. O(V + E)
    bool removeNode(int id);
    
    // Returns true if the node exists. O(V)
    bool hasNode(int id) const;

    // Adds an undirected road. false if a==b, node missing, dist<=0, or exists. O(V + degree)
    bool addEdge(int a, int b, double distanceKm);
    
    // Removes an undirected road. false if no such road. O(V + degree)
    bool removeEdge(int a, int b);
    
    // Changes distance and recomputes time. false if missing or dist<=0. O(V + degree)
    bool setEdgeDistance(int a, int b, double distanceKm);
    
    // Changes status and recomputes time. false if missing. O(V + degree)
    bool setStatus(int a, int b, RoadStatus s);
    
    // Changes average speed and recomputes EVERY road's time. false if kmh<=0. O(V + E)
    bool setAverageSpeed(double kmh);
    
    // Returns the current average speed. O(1)
    double averageSpeed() const;

    // Computes minutes based on distance and speed. O(1)
    int  computeTime(double distanceKm, RoadStatus s) const;

    // Retrieves info of road between a and b. false if missing. O(V + degree)
    bool getEdge(int a, int b, EdgeInfo& out) const;
    
    // Appends EVERY road at node id in ascending order of other endpoint. O(V + degree)
    void getEdgesOf(int id, std::vector<EdgeInfo>& out) const;
    
    // Appends NON-blocked roads at node id in ascending neighbor id. O(V + degree)
    void getNeighbors(int id, std::vector<Neighbor>& out) const;
    
    // Appends each road ONCE with a < b, ordered by a then b. O(V + E)
    void edgeList(std::vector<EdgeInfo>& out) const;
    
    // Appends all node ids in ascending order. O(V)
    void nodeIds(std::vector<int>& out) const;
    
    // Returns number of nodes. O(1)
    int  nodeCount() const;
    
    // Returns number of undirected roads. O(1)
    int  edgeCount() const;
    
    // Frees all memory and resets graph. O(V + E)
    void clear();

private:
    struct EdgeNode {
        int to;
        double distanceKm;
        RoadStatus status;
        int minutes;
        EdgeNode* next;
    };

    struct VertexNode {
        int id;
        EdgeNode* edges;
        VertexNode* next;
    };

    VertexNode* head_;
    int nodeCount_;
    int edgeCount_;
    double avgSpeed_;

    // Helper to find a vertex by id. O(V)
    VertexNode* findVertex(int id) const;
    
    // Helper to remove edge 'to' from vertex 'v'. O(degree)
    bool removeEdgeFromVertex(VertexNode* v, int to);
    
    // Helper to insert edge into vertex 'v' sorted by 'to'. O(degree)
    void insertEdgeIntoVertex(VertexNode* v, int to, double distanceKm, RoadStatus status, int minutes);
};

#endif
