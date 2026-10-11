# Phase 2B Spec: Graph, Dijkstra, Engine, scenarios and console menu

**Project:** ReliefNet, a C++ disaster-relief simulation (college data-structures micro-project)
**This phase:** the weighted graph, Dijkstra, the `Engine` that ties every structure together, the four demo scenarios, and a console menu. No server and no web UI (those are Phases 3 and 4).
**Depends on:** Phase 1 and Phase 2A, both committed and passing.
**Time budget:** about 40 minutes

> **Paste this into Antigravity:**
> "Read `docs/PHASE_2B_SPEC.md` and implement it exactly, in the order given in section 3. Follow the file names, function names and signatures as written. Do not add features, rename anything or touch anything outside this spec. Run the tests after each step and fix failures before starting the next step. The only earlier files you may change are the ones named in section 3. When finished, report the results as described in section 12. If anything in the spec is unclear or conflicts, stop and ask instead of guessing."

---

## 1. Why this matters (context)

Same as the earlier phases. This code will be explained line by line in an oral exam by a student who did not type it. So:

- Keep the code **simple and readable**. Prefer clarity over cleverness.
- Every public function gets a **one or two line comment** saying what it does and its time complexity.
- Every pointer manipulation step in the linked structures gets a short comment.
- The structures must be **real, hand-written** (linked vertex list with linked edge lists, hand-written Dijkstra). No graph libraries.
- The **Engine contains no printing and no file or network access.** The console menu and (later) the web server call the Engine. The Engine never calls them.

---

## 2. Hard rules

All earlier hard rules still apply (C++17, banned containers and `std::sort` in `engine/`, no globals, no `using namespace std;` in headers, destructors free everything, copying disabled, no external libraries, no files that are not listed). In addition:

1. **`std::vector` and `std::string` are allowed** in `engine/` as plain buffers and result holders (including the Dijkstra distance arrays). `std::map`, `std::set`, `std::unordered_map`, `std::list` and the others stay banned in `engine/`; they are allowed only in tests as oracles.
2. `<cmath>` is allowed (`std::lround`, `std::sqrt`). `<sstream>` is allowed for building trace strings and parsing scenario text. `std::min` and `std::max` are allowed.
3. **No `std::cout`, `printf`, `std::ifstream` or networking anywhere in `engine/`.** Only `console/console_main.cpp` prints, and only the tests and the console read files.
4. Constants such as `UNREACHABLE` may be `const` or `constexpr` at namespace scope. Mutable globals are still banned.
5. Distances are in **km** (`double`). Travel times are in **whole minutes** (`int`).
6. Camp IDs and graph node IDs are **the same number**. Node `0` is the depot.

---

## 3. Files, and the order to build them

```
engine/
  Graph.h        Graph.cpp        (NEW)  step 1
  Dijkstra.h     Dijkstra.cpp     (NEW)  step 2
  Engine.h       Engine.cpp       (NEW)  step 3
  Scenario.cpp                    (NEW)  step 4  (defines Engine::loadScenario)
console/
  console_main.cpp                (NEW)  step 5
data/
  scenario1_normal.txt     scenario1_normal.expected.txt     (NEW)  step 4
  scenario2_blockage.txt   scenario2_blockage.expected.txt   (NEW)  step 4
  scenario3_priority.txt   scenario3_priority.expected.txt   (NEW)  step 4
  scenario4_avl.txt        scenario4_avl.expected.txt        (NEW)  step 4
tests/
  test_graph.cpp  test_dijkstra.cpp  test_engine.cpp  test_scenarios.cpp   (NEW)
  run_tests.cpp      (CHANGE: add four suites)
  test_helpers.h     (CHANGE: add readTextFile, section 10)
build_and_test.bat   (CHANGE: add the new files)
build_console.bat    (NEW)
.gitignore           (CHANGE: add relief_console.exe)
README.md            (CHANGE: how to build and run the console menu)
```

**Order.** Build and test in this order, running `build_and_test.bat` after each step: **1** Graph and its tests, **2** Dijkstra and its tests, **3** Engine and its tests, **4** Scenario loader, the data files and their tests, **5** console menu. Do not start a step while the previous step has failing tests.

Do not change any existing function in earlier files.

---

## 4. Graph (`engine/Graph.h`, `engine/Graph.cpp`)

An undirected, weighted graph stored as an **adjacency list**: a linked list of vertices, and each vertex holds its own linked list of edges. Weights are never negative. A blocked road is **skipped** by routing; it is never given a huge weight.

```cpp
enum class RoadStatus { Open, Slow, Blocked };

struct EdgeInfo {            // a plain copy of one road, for callers
    int        a;            // the first endpoint (the node you asked about, or the smaller id in edgeList)
    int        b;            // the other endpoint
    double     distanceKm;
    RoadStatus status;
    int        minutes;      // travel time, see computeTime
};

struct Neighbor {            // one usable road out of a node
    int id;                  // the node at the other end
    int minutes;
};

class Graph {
public:
    Graph();
    ~Graph();
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    bool addNode(int id);                    // false if id < 0 or it already exists. O(V)
    bool removeNode(int id);                 // removes the node AND every road touching it. false if not found. O(V + E)
    bool hasNode(int id) const;              // O(V)

    bool addEdge(int a, int b, double distanceKm);
                                             // new road, status Open. false if: a == b, either node missing,
                                             // distanceKm <= 0, or a road between a and b already exists. O(V + degree)
    bool removeEdge(int a, int b);           // false if no such road. O(V + degree)
    bool setEdgeDistance(int a, int b, double distanceKm);
                                             // changes the length and recomputes the time. false if no such road or distanceKm <= 0.
    bool setStatus(int a, int b, RoadStatus s);   // false if no such road. Recomputes the time.
    bool setAverageSpeed(double kmh);        // false if kmh <= 0. Recomputes EVERY road's time. O(V + E)
    double averageSpeed() const;             // starts at 40.0

    int  computeTime(double distanceKm, RoadStatus s) const;
                                             // minutes = round(distanceKm / speed * 60), never less than 1.
                                             // Slow uses HALF the average speed. Open and Blocked use the full speed
                                             // (a blocked road is never used, so its number is only for display).
                                             // Use std::lround, so 7.5 rounds to 8.

    bool getEdge(int a, int b, EdgeInfo& out) const;   // false if no such road. (a,b) and (b,a) give the same road.
    void getEdgesOf(int id, std::vector<EdgeInfo>& out) const;
                                             // clears out, then appends EVERY road at node id (blocked ones too),
                                             // in ascending order of the other endpoint. out[i].a == id.
    void getNeighbors(int id, std::vector<Neighbor>& out) const;
                                             // clears out, then appends only the NON-blocked roads at node id,
                                             // in ascending order of neighbour id. This is what Dijkstra calls.
    void edgeList(std::vector<EdgeInfo>& out) const;
                                             // clears out, then appends each road ONCE with a < b, ordered by a then b.
    void nodeIds(std::vector<int>& out) const;     // clears out, then appends all node ids in ascending order
    int  nodeCount() const;
    int  edgeCount() const;                  // number of roads (each undirected road counts once)
    void clear();
};
```

Private parts: `struct EdgeNode { int to; double distanceKm; RoadStatus status; int minutes; EdgeNode* next; }` and `struct VertexNode { int id; EdgeNode* edges; VertexNode* next; }`.

Rules:
- The **vertex list is kept in ascending id order**. `addNode` inserts at the right place. (This makes every output deterministic and gives Dijkstra its tie rule.)
- Each vertex's **edge list is kept in ascending `to` order**.
- A road between `a` and `b` is stored as **two edge nodes**, one in each vertex's list. Every operation that changes a road (`setStatus`, `setEdgeDistance`, `removeEdge`, speed change) must update **both** copies. Comment this clearly.
- `removeNode` must first remove the edge node pointing at `id` from every neighbour's list, then free the node's own edge list, then unlink the vertex.
- `edgeCount()` is kept as a counter (or computed); either is fine. It must be correct after every operation.

---

## 5. Dijkstra (`engine/Dijkstra.h`, `engine/Dijkstra.cpp`)

Shortest travel time in **minutes** from one source node to every node. Simple **array version**, O(V squared).

```cpp
const int UNREACHABLE = -1;

struct DijkstraStep {
    int              node;       // the node picked in this step (smallest tentative time among unvisited reachable nodes)
    int              minutes;    // its final time
    std::vector<int> improved;   // ids of neighbours whose time got smaller in this step, in ascending id order
};

class Dijkstra {
public:
    void run(const Graph& g, int source);
         // Clears any earlier result, then computes times and predecessors from source. O(V squared).
         // If source is not a node of g, every node is UNREACHABLE and steps() is empty.
    int  getDistance(int node) const;         // minutes, or UNREACHABLE (also for unknown ids)
    bool isReachable(int node) const;         // false for UNREACHABLE and for unknown ids
    bool reconstructPath(int node, std::vector<int>& path) const;
         // On success: path = node ids from the source to node, e.g. 0, 1, 3, and returns true.
         // If node is unreachable or unknown: path is cleared and it returns false.
    const std::vector<DijkstraStep>& steps() const;   // one entry per picked node, in the order picked

private:
    int  pickMinimum() const;                 // index of the unvisited node with the smallest time; -1 if none is reachable
    void relax(const Graph& g, int uIndex, DijkstraStep& step);   // update the neighbours of node uIndex
    int  indexOf(int id) const;               // binary search in ids_; -1 if absent

    std::vector<int> ids_;      // node ids, ascending (copied from g.nodeIds)
    std::vector<int> dist_;     // dist_[i] is the time to ids_[i], or UNREACHABLE
    std::vector<int> prev_;     // prev_[i] is the INDEX of the predecessor, or -1
    std::vector<int> visited_;  // 0 or 1
    std::vector<DijkstraStep> steps_;
};
```

Rules:
- **Initialise:** every `dist_` is `UNREACHABLE`, the source is 0.
- **Loop:** repeat `pickMinimum`, mark it visited, record a step, `relax` its neighbours, until `pickMinimum` returns -1.
- **`pickMinimum`** ignores visited nodes and nodes whose time is `UNREACHABLE`. **On equal times the node with the smaller index (which is the smaller id) is picked.**
- **`relax`** uses `graph.getNeighbors`, so blocked roads are never used. It updates a neighbour only if the new total is **strictly smaller** (`<`), so on equal totals the path found first is kept. This is the tie rule, and it goes in the report.
- **Safe unreachable check:** never add to `UNREACHABLE`. Only relax from a node whose time is not `UNREACHABLE` (guaranteed by `pickMinimum`, but write a comment saying why).
- `indexOf` is a hand-written binary search. Add a comment explaining why the whole run is still O(V squared) (the binary search adds only O(E log V)).
- Add a comment at the top of `run` saying the heap version would be O((V+E) log V).

---

## 6. Engine (`engine/Engine.h`, `engine/Engine.cpp`)

The facade that ties the structures together. It owns one of each: `AVLTree` (camps), `Graph` (roads), `Dijkstra` (routes from the depot), `Queue` (incoming requests), `RequestList` (active requests), `DeliveryLog`. It owns the depot position, the depot stock, the average speed (inside the Graph), and **two counters**: `nextRequestId_` and `nextArrivalSeq_`. **Every request gets its `id` and its `arrivalSeq` from these counters, and from nowhere else** (the priority rule needs unique, increasing arrival numbers).

```cpp
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
};
```

### 6.1 What each function must do

All functions that can fail **check everything first and change nothing if any check fails** (all-or-nothing). Failure sets `err` and `message` and leaves the trace unchanged.

**`addCamp(c, linkTo)`** (`linkTo` is the list of node ids to connect to; 0 means the depot)
1. `!isValidCamp(c)` gives `Invalid`. Force the three received totals to 0 before inserting.
2. The id already exists gives `Duplicate`.
3. Any id in `linkTo` that is not 0 and not an existing camp gives `NotFound`. A link to the camp's own id, or the same id listed twice, gives `Invalid`. A link whose distance would be 0 (same position as the target) gives `Invalid`.
4. Then: AVL `insert`, graph `addNode`, graph `addEdge` for each link (length from positions, see 6.2), then rerun Dijkstra.
5. `value` = number of roads created.

**`editCamp(id, name, x, y, families)`**
1. Unknown id gives `NotFound`. Empty name or `families < 0` gives `Invalid`.
2. If the position changed, and any existing road of this camp would get length 0 at the new position, give `Invalid`.
3. Then: AVL `update`; for every road at this camp (including blocked ones) set the new length from the new positions; rerun Dijkstra. The id never changes.

**`deleteCamp(id, confirm)`**
1. `id < 1` gives `Invalid` (the depot cannot be deleted). Unknown id gives `NotFound`.
2. If `pendingCountForCamp(id) > 0` and `confirm` is false: give `ConfirmNeeded`, with `value` = the pending count and a message saying how many requests will be cancelled. Nothing changes.
3. Otherwise: AVL `remove`, `Queue::removeByCamp`, `RequestList::deleteByCamp`, graph `removeNode` (this removes all its roads), rerun Dijkstra. `value` = number of requests cancelled.

**`setDepotPosition(x, y)`**: same rules as moving a camp, applied to node 0. Rerun Dijkstra.

**`addRoad(a, b)`**: `a == b` is `Invalid`; an endpoint that is neither 0 nor a camp is `NotFound`; a road that already exists is `Duplicate`; length 0 is `Invalid`. Otherwise `Graph::addEdge`, rerun Dijkstra.

**`removeRoad`, `setRoadStatus`**: a road that does not exist is `NotFound`. Rerun Dijkstra.

**`setAverageSpeed(kmh)`**: `kmh <= 0` is `Invalid`. Rerun Dijkstra.

**`submitRequest(campId, r, quantity, urgency, families)`**
1. Build a `Request` with `id = nextRequestId_` and `arrivalSeq = nextArrivalSeq_`. If `!isValidRequest` it is `Invalid`.
2. If the camp is not in the AVL tree it is `NotFound`.
3. Only now `enqueue` it and **increment both counters**. A failed submit must not use up an id or an arrival number.
4. `value` = the new request id.

**`processIncoming()`**: if the queue is empty, `Empty`. Otherwise `dequeue` every request, `RequestList::insert` each one, then call `sortByPriority()` **once** at the end. `value` = how many were moved.

**`updateRequest(requestId, urgency, families, quantity)`**: the request must be in the **active list** (use `searchById`), otherwise `NotFound` (message says to process incoming first if it is still queued). If `RequestList::update` returns false the values are `Invalid`. After a successful update call `sortByPriority()`.

**`handleNext()`** serves one request. Steps:
1. Active list empty gives `Empty`.
2. Rerun Dijkstra (so the routes are fresh).
3. Walk the active list **in ranked order** (use `toVector()`). Pick the **first** request that can be served now: its camp is reachable from the depot **and** the depot stock of its resource is above 0. A request that fails either test is skipped, so it never blocks the requests behind it. In the normal case this is the top of the list (`getTop`).
4. If **no** request can be served: when at least one request had a reachable camp but its resource was out of stock, give `OutOfStock` (message names the resource of the first such request in ranked order, for example `Out of stock for food`). Otherwise every requested camp is unreachable: give `Unreachable` with a message that **starts with the exact text** `Unreachable: alternate delivery needed`. Nothing changes.
5. `delivered = min(request.quantity, stockOfThatResource)`. Take the route minutes from Dijkstra and the path from `reconstructPath`.
6. Subtract `delivered` from the depot stock. Call `AVLTree::addReceived` for the camp.
7. Append a `DeliveryRecord`: `requestId`, `campId`, `resource`, `quantityRequested` = the request's quantity at this moment, `quantityDelivered` = `delivered`, `totalMinutes`, `path`.
8. If `delivered == request.quantity`: `deleteById`. Otherwise (partial delivery): `update` the request with the same urgency and families and `quantity - delivered`, then `sortByPriority()`. The residual stays in the active list.
9. `value` = `delivered`. The message says what was delivered, the route and the minutes, and mentions the residual if there is one.

**`setStock(food, water, medicine)`**: any negative value is `Invalid`.

### 6.2 Road lengths

`distanceKm = sqrt((x1 - x2)^2 + (y1 - y2)^2)`. Node 0 uses the depot position; any other node uses its camp's position from the AVL tree. Write one private helper `positionOf(id, x, y)` and one private helper `distanceBetween(a, b)` and use them everywhere.

### 6.3 The operation trace

`lastTrace()` is a one-line string describing the last **successful** change. These parts are exact because tests and the viva use them:

- After `addCamp`: `AVL insert <id>: no rotation` or `AVL insert <id> caused <T> rotation at node <n>`, where `<T>` comes from `rotationName`.
- After `deleteCamp`: `AVL delete <id>: no rotation` or `AVL delete <id> caused <T> rotation at node <n>`. If there were several rotations, join them as `<T1> rotation at node <n1>; <T2> rotation at node <n2>`.
- After `editCamp`: `AVL update <id>: key unchanged, no rotation`.
- After `loadScenario` (section 7): `Scenario loaded: <C> camps, <R> roads, <Q> requests queued | AVL rotations: <list>` where `<list>` is every rotation from the whole load, in order, written `<T> at node <n>` and joined with `, `, or the word `none`.

Every other successful operation sets a short, sensible sentence (for example `Queue enqueue: request 4 for camp 2`, `Process incoming: 3 moved to the active list, merge sort run`, `Handle next: request 5 to camp 3, route 0 -> 1 -> 3, 10 min, delivered 20 of 20`, `Road 3-6 set to blocked, Dijkstra rerun`). It must never be empty after a success. Operations that change roads or camps append ` | Dijkstra rerun from depot` to their sentence.

### 6.4 Views

- `rankedRequests()`: for row `i`, `hasReason` is true when there is a row `i+1`, and `reason = rankReason(row[i], row[i+1])`.
- `routes()`: for every camp in ascending id, use the latest Dijkstra result.
- `pendingCountForCamp`: `Queue::toVector()` counted by camp, plus `RequestList::searchByCamp`.

---

## 7. Scenarios

### 7.1 Scenario text format (`Engine::loadScenario` in `engine/Scenario.cpp`)

The Engine receives the **text** of a scenario (the console or the test reads the file). It first calls `reset()`, then applies the lines **in order** through the normal public Engine functions. Rules:

- Split each line into words on spaces. Skip blank lines and lines whose first character is `#`.
- Commands (lower case):

```
depot X Y
speed KMH
stock FOOD WATER MEDICINE
camp ID NAME X Y FAMILIES            (NAME is one word, no spaces; the camp starts with no roads)
road A B                             (both ends must already be declared; the depot is node 0)
request CAMP RESOURCE QUANTITY URGENCY FAMILIES     (RESOURCE is food, water or medicine)
```

- `request` lines call `submitRequest`, so the requests wait in the **incoming queue**. The loader never calls `processIncoming`.
- Numbers must be read strictly: if a word is not a complete number, that is an error.
- **On any error** (unknown command, wrong number of words, bad number, or any Engine call that fails), call `reset()` and return `Err::Invalid` with a message that starts `line <N>: ` (N counts from 1) followed by the reason. A failed load leaves an empty, reset engine.
- On success `value` = number of requests queued, and the trace is set as in 6.3. Collect the AVL rotations from every `addCamp` call during the load to build that trace.

### 7.2 The scenario files

Create `data/scenario1_normal.txt` with exactly this content:

```
# Scenario 1: Normal. One depot, six camps, ten roads, six requests.
# Speed 60 km/h means 1 km = 1 minute, so every time can be checked by hand.
depot 0 0
speed 60
stock 1000 1000 1000
camp 1 Riverside 3 4 120
camp 2 Hilltop 6 0 80
camp 3 Eastgate 6 8 200
camp 4 Lakeview 9 4 150
camp 5 Westfield 12 0 60
camp 6 Northpoint 12 8 90
road 0 1
road 0 2
road 1 2
road 1 3
road 1 4
road 2 5
road 3 4
road 4 5
road 3 6
road 5 6
request 1 food 50 2 30
request 2 water 200 4 10
request 3 medicine 20 4 40
request 1 food 80 2 30
request 4 water 500 1 200
request 2 medicine 20 4 40
```

Create `data/scenario2_blockage.txt`: **identical to scenario 1 up to and including the last `road` line**, with the first comment line changed to `# Scenario 2: Blockage. Same map as scenario 1; block roads at run time.`, followed by exactly one request:

```
request 6 food 40 3 25
```

Create `data/scenario3_priority.txt`: **identical to scenario 1 up to and including the last `road` line**, with the first comment line changed to `# Scenario 3: Priority conflict. A big low-urgency request arrives before a small critical one.`, **except** the stock line, which must be `stock 100 100 100`. Then exactly these requests:

```
request 2 food 200 1 150
request 3 food 30 4 5
request 5 water 40 3 60
```

Create `data/scenario4_avl.txt` with exactly this content:

```
# Scenario 4: AVL demo. Camps are inserted in the order 10, 20, 30, 40, 50, 25 and cause rotations.
depot 0 0
speed 60
stock 1000 1000 1000
camp 10 Camp10 3 4 50
camp 20 Camp20 6 8 50
camp 30 Camp30 9 12 50
camp 40 Camp40 12 16 50
camp 50 Camp50 15 20 50
camp 25 Camp25 8 6 50
road 0 10
road 0 20
road 0 30
road 0 40
road 0 50
road 0 25
```

### 7.3 Expected results (hand-checked)

Copy each block below **verbatim** into the matching `.expected.txt` file in `data/`. The tests in section 10 check these same numbers.

**`scenario1_normal.expected.txt`**

```
Scenario 1: Normal (speed 60 km/h, so 1 km = 1 minute)

Routes from the depot (node 0):
camp 1:  5 min  path 0 -> 1
camp 2:  6 min  path 0 -> 2
camp 3: 10 min  path 0 -> 1 -> 3
camp 4: 11 min  path 0 -> 1 -> 4
camp 5: 12 min  path 0 -> 2 -> 5
camp 6: 16 min  path 0 -> 1 -> 3 -> 6

Dijkstra picks the nodes in this order: 0, 1, 2, 3, 4, 5, 6

Incoming queue, front to back: requests 1, 2, 3, 4, 5, 6
After Process incoming, ranked order: 3, 6, 2, 4, 1, 5
Reasons: 3 over 6 = arrival, 6 over 2 = families, 2 over 4 = urgency, 4 over 1 = quantity, 1 over 5 = urgency

Handle next, six times (depot stock starts at 1000 of each):
1. request 3 -> camp 3, medicine 20, route 0 -> 1 -> 3, 10 min
2. request 6 -> camp 2, medicine 20, route 0 -> 2, 6 min
3. request 2 -> camp 2, water 200, route 0 -> 2, 6 min
4. request 4 -> camp 1, food 80, route 0 -> 1, 5 min
5. request 1 -> camp 1, food 50, route 0 -> 1, 5 min
6. request 5 -> camp 4, water 500, route 0 -> 1 -> 4, 11 min
A seventh Handle next reports that there are no active requests.
Final depot stock: food 870, water 300, medicine 960
Delivery log: 6 records
Camp 1 received food 130. Camp 2 received water 200 and medicine 20. Camp 3 received medicine 20. Camp 4 received water 500.
```

**`scenario2_blockage.expected.txt`**

```
Scenario 2: Blockage (same map as scenario 1, one request: id 1, camp 6, food 40, urgency 3)

At load, routes are the same as scenario 1: camp 6 is 16 min via 0 -> 1 -> 3 -> 6.

Step 1: block road 3-6.
  Camp 6 changes to 20 min via 0 -> 2 -> 5 -> 6. Camps 1 to 5 are unchanged (5, 6, 10, 11, 12 min).
Step 2: also block road 5-6.
  Camp 6 is UNREACHABLE. Camps 1 to 5 are still 5, 6, 10, 11, 12 min.
Step 3: Process incoming, then Handle next.
  Result: "Unreachable: alternate delivery needed". The request stays in the active list. Nothing is delivered.
Step 4: open road 5-6 again, then Handle next.
  Camp 6 is 20 min via 0 -> 2 -> 5 -> 6. Food 40 is delivered. Final food stock 960.
```

**`scenario3_priority.expected.txt`**

```
Scenario 3: Priority conflict (stock 100 food, 100 water, 100 medicine)

Requests, in arrival order:
1. camp 2, food 200, urgency 1, families 150   (big, low urgency, arrives first)
2. camp 3, food 30, urgency 4, families 5      (small, critical)
3. camp 5, water 40, urgency 3, families 60

After Process incoming, ranked order: 2, 3, 1
Reasons: 2 over 3 = urgency, 3 over 1 = urgency

Handle next:
1. request 2 -> camp 3, delivered 30 of 30, route 0 -> 1 -> 3, 10 min. Food stock 70.
2. request 3 -> camp 5, delivered 40 of 40, route 0 -> 2 -> 5, 12 min. Water stock 60.
3. request 1 -> camp 2, delivered 70 of 200 (PARTIAL), route 0 -> 2, 6 min. Food stock 0.
   Residual 130 stays in the active list.
4. Handle next again: out of food. Nothing changes. The residual request (quantity 130) is still active.
```

**`scenario4_avl.expected.txt`**

```
Scenario 4: AVL demo (camps inserted in the order 10, 20, 30, 40, 50, 25)

Rotations, in order:
- insert 30: RR at node 10
- insert 50: RR at node 30
- insert 25: RL at node 20
(inserting 10, 20 and 40 causes no rotation)

Trace after load ends with: AVL rotations: RR at node 10, RR at node 30, RL at node 20

Final tree, preorder (id, height, balance):
30 (3, 0), 20 (2, 0), 10 (1, 0), 25 (1, 0), 40 (2, -1), 50 (1, 0)

Drawn:
ROOT 30
    L: 20
        L: 10
        R: 25
    R: 40
        R: 50

Routes: each camp has one direct road to the depot. Times in minutes: camp 10 = 5, 20 = 10, 30 = 15, 40 = 20, 50 = 25, 25 = 10.
```

---

## 8. Console menu (`console/console_main.cpp`)

A plain terminal program. It contains **no engine logic**: it reads input, calls `Engine`, and prints the returned `message`, then prints the line `Trace: <lastTrace()>`.

**Looks.** Print a banner at the start. Use `printf` width specifiers so tables have aligned columns. Use a separator line (for example a row of `-`) between sections. No colours in this phase.

**Input.** Read every answer with `std::getline`. A number must be read strictly: if the whole text is not a valid number, print `Not a valid number.` and ask again. **A blank answer cancels the current action** and returns to the menu.

**Main menu** (exactly these numbers):

```
 1  Load scenario (1 to 4)
 2  Add camp
 3  Edit camp
 4  Delete camp
 5  Search camp
 6  Submit request
 7  Process incoming
 8  Update request
 9  Handle next
10  Block / slow / open a road
11  Add road
12  Change average speed
13  Show data structures
14  Reset
 0  Exit
```

Notes:
- **1** reads `data/scenario<N>_<name>.txt` (the four file names in 7.2) from the current folder and passes the text to `loadScenario`. If the file cannot be opened, print a clear message and return to the menu.
- **2** asks for id, name (one line, may contain spaces), x, y, families, and a list of node ids to link to (numbers separated by spaces, 0 is the depot, blank for none).
- **4** shows the pending-request count if `ConfirmNeeded` comes back and asks `yes` or `no`. Only `yes` calls `deleteCamp` again with `confirm = true`.
- **5** uses `findCamp` and prints the camp with its three received totals.
- **10** asks for the two node ids and then `open`, `slow` or `blocked`.
- **13** opens a sub-menu: **a** incoming queue, **b** active list with a `reason` column, **c** delivery log, **d** AVL tree, **e** camps with received totals, **f** roads (a, b, km, status, minutes), **g** routes from the depot (a camp that is not reachable prints `Unreachable: alternate delivery needed`), **h** Dijkstra steps, **i** depot stock, depot position and average speed. Each shows the data **from the Engine's view functions**, never from a copy kept in the console.
- The AVL tree print (13 d) must look exactly like this, built from `avlSnapshot` (indent = 4 spaces times depth; the root says `ROOT`, other nodes say `L:` or `R:`):

```
ROOT 30  (h=3, bf=0)
    L: 20  (h=2, bf=0)
        L: 10  (h=1, bf=0)
        R: 25  (h=1, bf=0)
    R: 40  (h=2, bf=-1)
        R: 50  (h=1, bf=0)
```

- The Dijkstra steps print (13 h) shows one line per step: `pick node <n> at <m> min, improved: <ids or none>`.

---

## 9. Build

Update `build_and_test.bat` so its single explicit `g++` command becomes (no wildcards):

```
g++ -std=c++17 -Wall -Wextra -g -I engine -I tests ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp engine\AVLTree.cpp ^
  engine\Graph.cpp engine\Dijkstra.cpp engine\Engine.cpp engine\Scenario.cpp ^
  tests\test_types.cpp tests\test_queue.cpp tests\test_request_list.cpp tests\test_sorting.cpp ^
  tests\test_delivery_log.cpp tests\test_avl.cpp tests\test_graph.cpp tests\test_dijkstra.cpp ^
  tests\test_engine.cpp tests\test_scenarios.cpp tests\run_tests.cpp -o run_tests.exe
```

Create `build_console.bat` with one explicit command that builds the menu program, stops with a clear message if it fails, and then tells the user to run `relief_console.exe` from the repo root (the scenario files are found relative to it):

```
g++ -std=c++17 -Wall -Wextra -g -I engine ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp engine\AVLTree.cpp ^
  engine\Graph.cpp engine\Dijkstra.cpp engine\Engine.cpp engine\Scenario.cpp ^
  console\console_main.cpp -o relief_console.exe
```

Zero warnings under `-Wall -Wextra` for both commands. Update `README.md` with both commands.

---

## 10. Tests

In `run_tests.cpp` add, after the AVL suite: `runSuite("Graph", runGraphTests);`, `runSuite("Dijkstra", runDijkstraTests);`, `runSuite("Engine", runEngineTests);`, `runSuite("Scenarios", runScenarioTests);`. Declare them the same way the existing suite functions are declared. The leak counter checks each suite automatically.

In `test_helpers.h` add one inline helper `std::string readTextFile(const char* path)` that reads a whole file (tests run from the repo root, so paths look like `"data/scenario1_normal.txt"`). If the file cannot be opened it returns an empty string, and the calling test must then record a failure with `CHECK`.

### 10.1 Graph (`test_graph.cpp`)
1. New graph: `nodeCount() == 0`, `edgeCount() == 0`, `averageSpeed() == 40.0`.
2. `addNode`: ids 5, 2, 9, 0 inserted in that order; `nodeIds` returns 0, 2, 5, 9. A duplicate and a negative id return false. `hasNode` is right for present and absent ids.
3. `addEdge`: succeeds between existing nodes; returns false for `a == b`, a missing node, `distanceKm = 0`, a negative distance, and a duplicate road in either order (`(a,b)` then `(b,a)`). `edgeCount` is right after each.
4. Undirected: after `addEdge(1, 2, 7)`, `getEdge(1, 2)` and `getEdge(2, 1)` both succeed and report the same distance and status.
5. `computeTime`, with `setAverageSpeed(60)`: `(7, Open)` is 7, `(7, Slow)` is 14, `(7, Blocked)` is 7, `(0.4, Open)` is 1 (minimum). With `setAverageSpeed(40)`: `(10, Open)` is 15, `(10, Slow)` is 30, `(5, Open)` is 8 (7.5 rounds up).
6. `setStatus`: changes both directions (check `getEdge(a,b)` and `getEdge(b,a)`); `Slow` doubles the minutes, `Open` restores them; returns false for a missing road.
7. `getNeighbors` skips `Blocked` roads, and its output is in ascending neighbour id even if the roads were added in another order. `getEdgesOf` still lists the blocked road.
8. `setAverageSpeed`: after building a road of 7 km at speed 60 (7 min), `setAverageSpeed(30)` makes it 14 min. `setAverageSpeed(0)` and `(-5)` return false and change nothing.
9. `setEdgeDistance` recomputes the time (7 km at speed 60, set to 12, gives 12 min); returns false for a missing road and for distance <= 0.
10. `removeEdge`: the road disappears from both ends; `edgeCount` drops by 1; a second remove returns false.
11. `removeNode`: build a star (node 0 joined to 1, 2, 3) plus a road 1-2. Remove node 1: `nodeCount` 3, `edgeCount` 2, `getEdge(0, 1)` and `getEdge(1, 2)` fail, `getEdgesOf(2)` no longer lists node 1. Removing a missing node returns false.
12. `edgeList` lists each road once with `a < b`, ordered by a then b.
13. `clear` empties the graph, which can then be reused.
14. Stress: a ring of 300 nodes (each joined to the next, and the last to the first): `edgeCount() == 300`; then remove every node. No leaks (the suite counter checks it).

### 10.2 Dijkstra (`test_dijkstra.cpp`)
Set `setAverageSpeed(60)` in every fixture so **1 km is 1 minute**. Use node 0 as the source.

**Classic graph** (nodes 0 to 5). Roads `a-b: km`: 0-1: 7, 0-2: 9, 0-5: 14, 1-2: 10, 1-3: 15, 2-3: 11, 2-5: 2, 3-4: 6, 4-5: 9.

| Node | Minutes | Path |
|---|---|---|
| 0 | 0 | 0 |
| 1 | 7 | 0, 1 |
| 2 | 9 | 0, 2 |
| 3 | 20 | 0, 2, 3 |
| 4 | 20 | 0, 2, 5, 4 |
| 5 | 11 | 0, 2, 5 |

1. All six rows above, with `reconstructPath` returning true.
2. **Steps** on the classic graph: `steps()` has 6 entries. The picked nodes in order are 0, 1, 2, 5, 3, 4 with minutes 0, 7, 9, 11, 20, 20. The `improved` lists are: step 1 `[1, 2, 5]`, step 2 `[3]`, step 3 `[3, 5]`, step 4 `[4]`, step 5 `[]`, step 6 `[]`.
3. **Blocked road changes the path.** Block road 2-5. Expected minutes: node 1 is 7, node 2 is 9, node 3 is 20 (path 0, 2, 3), node 4 is 23 (path 0, 5, 4), node 5 is 14 (path 0, 5).
4. **Slow road keeps the path but raises the total.** Set road 0-1 to `Slow`. Node 1 becomes 14 minutes and its path is still 0, 1. Nodes 2, 3, 4, 5 keep their times from the table (9, 20, 20, 11).
5. **Unreachable camp.** Block 2-3, 1-3 and 3-4. Node 3 is `UNREACHABLE`: `isReachable(3)` false, `getDistance(3) == UNREACHABLE`, `reconstructPath(3, p)` returns false and clears `p`. The others keep their times (7, 9, 20, 11 for nodes 1, 2, 4, 5).
6. **Equal-weight tie.** Nodes 0 to 3; roads 0-1: 2, 0-2: 2, 1-3: 3, 2-3: 3. Node 3 is 5 minutes and the path is **0, 1, 3** (the smaller id wins the tie).
7. **Edge cases.** A graph with only node 0: node 0 is 0 minutes, path is just 0. `getDistance(99)` is `UNREACHABLE` and `isReachable(99)` is false. `run` with a source that is not in the graph makes everything unreachable and leaves `steps()` empty. A node with no roads is unreachable.
8. **Re-running.** After changing the graph, `run` again gives the new answers (nothing left over from the first run).
9. **Randomised oracle.** Use `std::mt19937` with seed 42. Build a graph of 25 nodes (ids 0 to 24, added in shuffled order), roads between random pairs with probability about 0.15 and a random whole-number length of 1 to 20 km, and give each road a random status (about 20 percent `Blocked`, 20 percent `Slow`, the rest `Open`). At speed 60 an Open road costs its km in minutes and a Slow road costs twice that. Compute the correct answers with a **Bellman-Ford** loop written in the test, and check that every distance matches (unreachable matches `UNREACHABLE`). For every reachable node also check the reconstructed path: it starts at 0, ends at the node, uses only non-blocked roads, and its minutes add up to `getDistance`. Repeat for seeds 1, 2, 3, 4 and 42.

### 10.3 Engine (`test_engine.cpp`)
Use a small **mini world** built through the public functions (each numbered test below builds its own fresh one), with `setAverageSpeed(60)`: depot at (0, 0); camp 1 at (3, 4) linked to {0}; camp 2 at (6, 8) linked to {1}. (Road 0-1 is 5 min, road 1-2 is 5 min.)

1. **Fresh engine and reset.** `campCount() == 0`, queue empty, stock is 500/500/500, `averageSpeed() == 40`, `depotX() == 0`. After building the mini world and calling `reset()`, everything is empty again and the next submitted request has id 1.
2. **addCamp.** Camp 1 and camp 2 succeed. Routes: camp 1 is 5 min via 0, 1; camp 2 is 10 min via 0, 1, 2. The result `value` is the number of roads created (1). Failures, each leaving `campCount()` and `roads().size()` unchanged: `Duplicate` (camp 1 again), `Invalid` (id 0; empty name; negative families; an x or y that is NaN, infinite or beyond 1,000,000 km in either direction; a link to its own id; the same link twice; a camp placed on the exact position of the node it links to), `NotFound` (a link to camp 99).
3. **addCamp trace.** On a fresh engine add camps 10, 20, 30 (all linked to the depot, at distinct positions). The trace after adding 10 and after 20 **starts with** `AVL insert <id>: no rotation`. After adding 30 it **starts with** `AVL insert 30 caused RR rotation at node 10` (more text may follow after ` | `; check the start of the string). `lastRotations()` has one entry (RR at 10).
4. **editCamp.** Rename camp 2 and change its families: stored. Move camp 2 to (9, 12): the road 1-2 becomes 10 min, so the route to camp 2 is 15 min. Moving camp 2 onto (3, 4) (the position of camp 1, which it is linked to) returns `Invalid` and changes nothing. Unknown id is `NotFound`; empty name is `Invalid`.
5. **Roads.** Block 0-1: camps 1 and 2 are unreachable. `addRoad(0, 2)` makes camp 2 reachable at 10 min via 0, 2 (the distance is 10 km), while camp 1 is reachable via 0, 2, 1 at 15 min. `addRoad(0, 2)` again is `Duplicate`; `addRoad(1, 1)` is `Invalid`; `addRoad(0, 99)` is `NotFound`; `setRoadStatus(1, 5, ...)` on a missing road is `NotFound`; `removeRoad` removes it. `setAverageSpeed(30)` doubles every minute figure; `setAverageSpeed(0)` is `Invalid`.
6. **submitRequest.** Unknown camp is `NotFound`; quantity 0, urgency 5, families 0 are each `Invalid`. **None of the failures uses up a number:** the first successful submit afterwards has id 1 and `arrivalSeq` 1, the next id 2 and `arrivalSeq` 2. `value` is the new id. `queueContents()` is in arrival order.
7. **processIncoming.** Empty queue gives `Empty`. With two queued requests (id 1: camp 1, food 100, urgency 2, families 10; id 2: camp 2, water 50, urgency 4, families 5) it moves both (`value == 2`), the queue is empty, and `rankedRequests()` is id 2 then id 1, with `hasReason` true and `reason == RankKey::Urgency` on the first row and `hasReason` false on the last.
8. **updateRequest.** Raise request 1 to urgency 4 with families 50: it becomes first. An unknown id is `NotFound`; urgency 0 is `Invalid` and changes nothing. A request that is still in the incoming queue is `NotFound`.
9. **handleNext, full delivery.** From step 7 (stock 500 each): the first call serves request 2: `value == 50`, path 0, 1, 2, `totalMinutes` 10, water stock 450, camp 2 `receivedWater == 50`, delivery log has 1 record whose fields all match, active list has 1 request. The second call serves request 1 (food 100). The third call returns `Empty`.
10. **handleNext, partial and out of stock.** `setStock(30, 500, 500)`. Submit and process camp 1, food 100. `handleNext` delivers 30 (`value == 30`), the request stays with quantity 70, food stock is 0, the log record says requested 100 and delivered 30. The next `handleNext` returns `OutOfStock` and nothing changes.
11. **handleNext skips an unreachable camp.** Add camp 3 at (20, 20) with no links (`linkTo` empty). Submit an urgency-4 request for camp 3 and an urgency-1 request for camp 1. After processing, camp 3's request is first but camp 3 is unreachable. `handleNext` serves camp 1's request, and camp 3's request is still in the active list. The next `handleNext` returns `Unreachable` whose message starts with `Unreachable: alternate delivery needed`, and nothing changes (log size, stock and list size are the same).
12. **deleteCamp.** With one request for camp 1 in the incoming queue and one in the active list: `deleteCamp(1, false)` returns `ConfirmNeeded` with `value == 2` and changes nothing. `deleteCamp(1, true)` succeeds with `value == 2`, the queue and list no longer hold camp 1 requests, `findCamp(1)` is false, the graph has no roads at node 1, and the trace starts with `AVL delete 1`. `deleteCamp(0, true)` is `Invalid`; `deleteCamp(99, true)` is `NotFound`.
13. **Counters.** After a reset the counters restart at 1. Arrival numbers are strictly increasing across all successful submits, and `RequestList::isSortedByPriority()` holds after every `processIncoming`, `updateRequest` and `handleNext` (check through `rankedRequests()` and `higherPriority`).
14. **Trace.** Every successful mutating call leaves `lastTrace()` non-empty.

### 10.4 Scenarios (`test_scenarios.cpp`)
Load each file with `readTextFile` and `loadScenario`. All numbers below come from section 7.3.

1. **Scenario 1.** `ok()`, `value == 6`, 6 camps, `roads().size() == 10`. `routes()` match the table in 7.3 (minutes and paths, all six reachable). The picked order in `dijkstra().steps()` is 0, 1, 2, 3, 4, 5, 6. `queueContents()` ids are 1, 2, 3, 4, 5, 6. After `processIncoming`, the ranked ids are **3, 6, 2, 4, 1, 5** and the reasons for the first five rows are **arrival, families, urgency, quantity, urgency**. Six `handleNext` calls deliver, in order, the six rows in 7.3 (check request id, camp, delivered, minutes and path). The seventh returns `Empty`. Final stock is 870 / 300 / 960, the log has 6 records, and the camp received totals match the last line of the expected file.
2. **Scenario 2.** Loads with `value == 1`. Routes equal scenario 1. `setRoadStatus(3, 6, Blocked)`: camp 6 is 20 min via 0, 2, 5, 6 and camps 1 to 5 are 5, 6, 10, 11, 12. Then `setRoadStatus(5, 6, Blocked)`: camp 6 is unreachable (`reachable == false`, `minutes == UNREACHABLE`, empty path) and camps 1 to 5 are unchanged. `processIncoming`, then `handleNext` returns `Unreachable` with the required message, the list still has 1 request, the log is empty, the stock is unchanged. Then `setRoadStatus(5, 6, Open)` and `handleNext`: delivers 40, path 0, 2, 5, 6, 20 minutes, food stock 960.
3. **Scenario 3.** Loads with `value == 3`. After `processIncoming` the ranked ids are 2, 3, 1 and the reasons are urgency, urgency. Four `handleNext` calls: (1) request 2, delivered 30, 10 minutes, path 0, 1, 3, food stock 70; (2) request 3, delivered 40, 12 minutes, path 0, 2, 5, water stock 60; (3) request 1, `value == 70`, 6 minutes, path 0, 2, food stock 0, the request is still active with quantity 130, and the log record says requested 200 and delivered 70; (4) `OutOfStock`, with the request still active at quantity 130.
4. **Scenario 4.** Loads with `value == 0`. The trace contains exactly `AVL rotations: RR at node 10, RR at node 30, RL at node 20`. `avlSnapshot` preorder is 30, 20, 10, 25, 40, 50 with (height, balance) of (3, 0), (2, 0), (1, 0), (1, 0), (2, -1), (1, 0). Route minutes for camps 10, 20, 30, 40, 50, 25 are 5, 10, 15, 20, 25, 10, each via 0 then the camp.
5. **A load replaces everything.** Load scenario 1, load scenario 4, and check that camps 1 to 6 are gone and the queue is empty.
6. **Bad scenario text** (each leaves an empty reset engine, `Err::Invalid`, and a message starting `line <N>:` with the right N). Use small inline strings: an unknown command on line 2; `road 0 7` before camp 7 exists; a duplicate camp id; `camp 1 Name abc 4 5` (bad number); `camp 1 Name 3` (too few words); `request 9 food 10 2 5` for an unknown camp; `request 1 rice 10 2 5` (unknown resource). Blank lines and `#` comment lines never cause an error.
7. **Expected files exist.** Each of the four `.expected.txt` files is non-empty when read with `readTextFile`.

---

## 11. Style checklist

- Every public function has its short comment with complexity.
- Every pointer step in `Graph.cpp` has a short comment. The "two edge nodes per road" rule is explained in a comment in `Graph.cpp`.
- `Dijkstra.cpp` has a comment explaining the tie rule and the O(V squared) argument.
- No function longer than about 50 lines, except `main` in the console file may be longer if it is only the menu loop.
- No printing, files or networking in `engine/`.
- No raw `new` or `delete` outside `Graph.cpp`.

---

## 12. Definition of done and what to report back

**Done when all of these are true:**

- [ ] Every file in section 3 exists, with the names and signatures in this spec.
- [ ] Zero compiler warnings for both build commands.
- [ ] All tests pass (every earlier suite and the four new ones), and the run reports `0 failures`.
- [ ] The allocation counter reports no leaks in any suite.
- [ ] Nothing under `engine/` uses the banned containers or algorithms, and nothing under `engine/` prints or reads files.
- [ ] No earlier function was modified.
- [ ] `relief_console.exe` runs. In it: menu option 1 loads scenarios 1 to 4, and option 13 d shows the scenario 4 tree exactly as in section 8.
- [ ] Changes are committed with the message `Phase 2B: graph, dijkstra, engine, scenarios, console menu` and pushed.

**Report back with:**

1. The exact test output (the final line with the check and failure counts).
2. The list of files created and the list of files changed.
3. A copy of what the console prints for this sequence: option 1, choose 4, then option 13, then d.
4. Anything that was ambiguous or that you changed from this spec, and why. If nothing, say "no deviations".
5. The result of this search, which should return nothing: `Select-String -Path engine\* -Pattern "std::(queue|list|forward_list|priority_queue|deque|map|set|sort|unordered_map|cout|cin|ifstream|ofstream)"` and `Select-String -Path engine\* -Pattern "printf"` (PowerShell)
