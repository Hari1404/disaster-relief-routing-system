# ReliefNet: Project Plan

**Project:** Data Structure-Based Disaster Relief Distribution and Routing System (C++ micro-project)
**Format:** C++ engine + local browser interface, fully offline
**Time budget:** about 2 to 3 hours of build time, four phases
**Status:** planning complete except the open decisions in section 14

---

## 1. Goal and ground rules

Build a flood-relief simulation that runs on your laptop. You open a browser page on `localhost`, click around, and every action is handled by hand-written C++ data structures. In the viva you can open the code and show exactly where each structure is used.

**Ground rules**

1. **Every required structure is hand-written C++:** queue, linked list, AVL tree, weighted graph, Dijkstra, merge sort. No `std::map`, `std::list`, `std::queue`, `std::priority_queue` or graph libraries for these. `std::vector` and `std::string` are fine as plain buffers.
2. **Libraries are for transport and display only:** a single-header HTTP server, a single-header JSON library, and plain HTML/CSS/JS.
3. **The browser never keeps its own copy of the data.** It draws whatever the C++ engine sends. If the browser held its own queue, the queue would be fake.
4. **The engine knows nothing about the interface.** It has no printing and no networking. A test driver, a console menu and the web server all call the same engine.
5. **No AI in the product.** The research files all agree the core is deterministic. AI was only used as a coding helper.
6. **Only synthetic data.** IDRN data is restricted to authorised officials, so the report says the data is invented.

---

## 2. What the research files say

Four files were reviewed: your research findings (two identical copies), Grok's report, Gemini Pro and Gemini Pro High.

**All of them agree on:**
- AI is not needed. Dijkstra and AVL are deterministic and cannot be improved by a model.
- Dijkstra solves one point-to-point shortest path. It does not solve multi-stop routing, which is NP-hard.
- The structures must be hand-written, and the STL question should be confirmed with the course-in-charge.
- Unreachable camps must be handled explicitly, not shown as a huge number.
- Playwright MCP is the wrong test tool here. C++ tests prove the algorithms, not a browser.

**Where they differ, and what we chose**

| Idea | Source | Decision |
|---|---|---|
| Pure console app is the safest baseline | Grok, Gemini High | Our engine is independent of the interface, so a console fallback always exists |
| C++ core + local API + web view | Gemini Pro, your findings (optional) | **Chosen**, because you want an interactive interface |
| Priority queue as a min-heap | Gemini Pro | **Not used.** Proposal wants a FIFO queue plus sorting |
| Weighted-sum score with a waiting term | Your findings doc | **Replaced** by a lexicographic rule (section 7) |
| Lexicographic rule (urgency, then families, then quantity, then arrival) | Grok | **Chosen**, simpler to explain and defend |
| Integer minutes for travel time | Grok | **Chosen**, avoids float tie problems |
| HDX/CSV dataset, 10,000-node benchmarks, GoogleTest, Valgrind | Gemini Pro | **Skipped** for the 2-3 hour budget |
| Local or cloud LLM to parse requests | Gemini Pro/High | **Rejected** |
| Real map tiles | Grok warns against | **Rejected.** Drawn map, no tile servers |

---

## 3. Decisions locked in

| Area | Decision |
|---|---|
| Run model | One C++ program serves a page at `localhost:8080`. Offline. |
| Libraries | `cpp-httplib` (server), `nlohmann/json` (JSON). Both single headers. |
| Hosting | None. GitHub is for storing code only. No Vercel. |
| Map | Drawn SVG network. Click to place camps. |
| Roads | Straight lines between camps. |
| Travel time | `time = distance / average speed`, in whole minutes |
| Road status | Open, Slow (reduced speed), Blocked (never used) |
| Depot | One depot, reserved node ID 0, not stored in the AVL tree |
| Vehicles | Out of scope. Stated as a simplification in the report. |
| Camp ID and graph node | Same number. One deliberate link, not two copies. |
| Algorithm for routes | Dijkstra with the simple array version, O(V squared) |

---

## 4. Architecture

```
 Browser (HTML / CSS / JS, drawing only)
        |  JSON over HTTP, localhost
 Server  (cpp-httplib, thin: parse request -> call engine -> return snapshot)
        |
 Engine  (pure C++, no I/O)   <--  Test driver   <--  Console menu (fallback)
   Queue | RequestList | DeliveryLog | AVLTree | Graph | Dijkstra | MergeSort
```

**Folder layout**

```
reliefnet/
  engine/      Types.h  Queue.h/.cpp  RequestList.h/.cpp  DeliveryLog.h/.cpp
               AVLTree.h/.cpp  Graph.h/.cpp  Dijkstra.h/.cpp  Sorting.h/.cpp
               Engine.h/.cpp   (the facade that ties the structures together)
  tests/       one test file per module + run_tests.cpp
  server/      main.cpp  httplib.h  json.hpp
  web/         index.html  style.css  app.js
  data/        scenario files (section 10)
  docs/        PROJECT_PLAN.md  VIVA_CHEATSHEET.md
  README.md
```

**Build:** `g++ -std=c++17`. Run tests with `-fsanitize=address,undefined` to catch memory errors. Every structure has a destructor that frees its nodes.

---

## 5. The screen

One page, four areas.

1. **Map (centre).** Camps are dots, the depot is marked, roads are lines labelled with travel time. Click empty space to place a camp. Click a road to cycle Open, Slow, Blocked. The current route is highlighted. Blocked roads are drawn dashed.
2. **Camps panel (left).** Table of camps with add, edit, search by ID and delete. Duplicate IDs are rejected. Deleting a camp with pending requests asks for confirmation, then cancels those requests.
3. **Requests panel (right).** Submit form: camp, resource type, quantity, urgency, families affected. Below it, the **incoming queue** and the **ranked active list**, each row showing why it ranks where it does. Buttons: **Process incoming** and **Handle next**.
4. **Data Structures panel (bottom).** Tabs for: Queue, Active list, Delivery log, AVL tree (drawn with heights and balance factors), Dijkstra steps. At the top, an **operation trace** line such as "AVL insert 103 caused RL rotation at node 101". It updates after every action.

Also on screen: an **average speed** control, a **scenario** dropdown, and a **reset** button.

---

## 6. Every data structure and every operation

### 6.1 Queue: incoming requests (CO-2)
Arrival order, first in first out. Linked nodes with head and tail pointers.

| Operation | Purpose | Time |
|---|---|---|
| `enqueue` | New request joins the back | O(1) |
| `dequeue` | Oldest request leaves the front | O(1) |
| `peek` | Look at the front | O(1) |
| `isEmpty`, `size` | Status | O(1) |
| `display` / `snapshot` | Show front to back | O(n) |
| `removeByCamp` | Drop pending requests when a camp is deleted | O(n) |

### 6.2 Linked list A: active requests (CO-2, CO-3)
Requests that were dequeued and are waiting to be served. Records change often, so links are easy to re-point.

| Operation | Purpose |
|---|---|
| `insert` | Add a dequeued request |
| `searchById`, `searchByCamp` | Linear search |
| `update` | Change urgency, families or quantity, then re-sort |
| `deleteById`, `deleteByCamp` | Remove when handled, cancelled, or the camp is deleted |
| `getTop` | Highest-priority request (the head after sorting) |
| `traverse` / `snapshot`, `size`, `isEmpty` | Viewing and status |
| `mergeSort` | Hand-written, stable, by the priority rule |

### 6.3 Linked list B: delivery log
| Operation | Purpose |
|---|---|
| `append` | Record a delivery: request, camp, route, total minutes, quantity delivered |
| `traverse` / `snapshot`, `size` | History |
| `searchByCamp` | Deliveries to one camp |

### 6.4 AVL tree: camp records, keyed by unique Camp ID (CO-4)
Camp record: ID, name, x, y, families, and totals received (food, water, medicine). Balance factor is defined as **left height minus right height**.

| Operation | Purpose |
|---|---|
| `insert` | Add a camp, reject duplicates, rebalance |
| `search` | Find by ID, O(log n) |
| `update` | Edit name, families, position. The key never changes. |
| `delete` | Leaf, one child, two children (in-order successor), then rebalance |
| `height`, `balanceFactor` | Needed for balancing |
| `rotateLeft`, `rotateRight` | Building blocks |
| LL, RR, LR, RL cases | The four imbalance fixes |
| `inorder` | Camps sorted by ID |
| `findMin` | Helper for deletion |
| `count`, `isEmpty` | Status |
| `snapshot` | Nested structure with heights and balance factors, for drawing |

### 6.5 Weighted graph: roads (CO-5)
Adjacency list: a linked list of vertices, each holding a linked list of edges. Each edge stores distance, status and travel time.

| Operation | Purpose |
|---|---|
| `addNode` | Runs when a camp is added |
| `removeNode` | Runs when a camp is deleted, removes all its roads |
| `addEdge` | Connect two nodes. Distance comes from their positions. |
| `removeEdge` | Delete a road |
| `setStatus` | Open, Slow, Blocked |
| `setAverageSpeed` | Global speed. Every travel time is recomputed. |
| `computeTime` | `round(distanceKm / speed * 60)`, minimum 1 minute. Slow roads use half speed. |
| `recomputeEdges` | Runs when a camp is moved |
| `getNeighbors` | Used by Dijkstra, skips blocked edges |
| `isReachable` | Whether a camp can be reached from the depot |
| `nodeCount`, `edgeCount`, `snapshot` | Status and drawing |

Rules: weights are never negative. A blocked road is **skipped**, never given a huge weight. Roads are undirected.

### 6.6 Dijkstra (CO-5)
Shortest travel time from the depot to every camp.

| Step | Purpose |
|---|---|
| `init` | All times unreachable, depot 0 |
| `pickMinimum` | Unvisited node with the smallest time |
| `relax` | Update neighbours through that node |
| Skip blocked edges | Blocked roads are never used |
| Safe unreachable check | Never add to the "infinity" value |
| `reconstructPath` | Rebuild the route from the predecessor array |
| `getDistance` | Total minutes, or "unreachable" |
| `recordSteps` | Save each step for the Data Structures panel |

Tie rule: when two paths have equal total time, the one found first (lower node ID processed first) is kept. This is stated in the report.
Reruns when: a request is handled, a road changes, speed changes, or a camp is added, moved or removed.
An unreachable camp shows **"Unreachable: alternate delivery needed"**.

### 6.7 Searching and sorting (CO-3)
- **Merge sort** (hand-written, stable) on the active request list.
- **Linear search** for requests by ID or camp.
- **AVL search** for camps.

---

## 7. Priority rule

Changed from the earlier plan after the research review. A single weighted score can let a large low-urgency request beat a small critical one. A lexicographic rule has no weights to defend.

Requests are ordered by, in this sequence:

1. **Urgency** (Critical 4, High 3, Medium 2, Low 1). Higher first.
2. **Families affected.** More first.
3. **Quantity required.** More first.
4. **Arrival sequence number.** Earlier first. This is why the queue still matters.

The interface shows, for each row, the first key where it differs from the row below it, as the "reason" for its rank.

**Known limitation to state in the viva:** a low-urgency request can wait indefinitely if critical requests keep arriving. An aging term is a stretch goal.

Arrival uses a counter, not the clock, so tests are repeatable.

---

## 8. Stock and delivery

The proposal says a delivery updates camp, stock, vehicle and request records. To cover this at small cost:

- The depot has a stock count for **food, water and medicine**.
- A request names one resource type and a quantity.
- **Handle next:** deliver `min(requested, stock)`. If stock is short, a partial delivery happens and the **residual stays in the active list**. The camp's received totals and the depot stock update, and the delivery goes into the log.
- Vehicles are not modelled. This is stated as a simplification.

---

## 9. Every user action traced

| Action | Structures touched |
|---|---|
| **Add camp** | AVL `insert`, graph `addNode`, graph `addEdge` for each chosen link, Dijkstra rerun |
| **Edit camp** | AVL `search` + `update`, `recomputeEdges` if moved, Dijkstra rerun |
| **Delete camp** | AVL `search` then `delete`, queue `removeByCamp`, list `deleteByCamp`, graph `removeNode`, Dijkstra rerun |
| **Search camp** | AVL `search` |
| **Submit request** | AVL `search` (camp must exist), validate, queue `enqueue` |
| **Process incoming** | queue `dequeue`, list `insert`, `mergeSort` |
| **Update request** | list `searchById` + `update`, `mergeSort` |
| **Handle next** | list `getTop`, Dijkstra path, stock check, camp update, list `deleteById` (or keep residual), log `append` |
| **Block / slow / open road** | graph `setStatus`, Dijkstra rerun, re-route |
| **Add road** | graph `addEdge`, Dijkstra rerun |
| **Change speed** | graph `setAverageSpeed`, Dijkstra rerun |
| **Load scenario** | resets everything, then AVL inserts, graph build, enqueues |

**Validation (edge cases examiners probe):** non-numeric input, negative or zero values, duplicate camp IDs, request for an unknown camp, empty structures, unreachable camp, deleting a camp with pending requests.

---

## 10. Demo scenarios

Four hand-checked scenarios shipped in `data/`, each with its expected result written next to it.

1. **Normal.** One depot, six camps, about ten roads. Hand-computed shortest paths and a hand-sorted request list.
2. **Blockage.** Same graph. Block one road: the route changes. Block another: one camp becomes unreachable.
3. **Priority conflict.** A large low-urgency request arrives before a small critical one. The critical one is served first. Stock is short, so there is a partial delivery.
4. **AVL demo.** Insert camp IDs in sequence (10, 20, 30, 40, 50, 25). Rotations are visible in the tree view and the operation trace.

**5-minute viva script:** load scenario 1, show the map and the AVL tree, submit requests and watch the queue, process them and show the ranked list with reasons, handle next and show the route, block a road and show the new route, run scenario 4 for the rotations, then open the code and walk through `enqueue`, `dequeue`, `insert`, `rotate`, `dijkstra`.

---

## 11. The bridge: HTTP endpoints

The interface decides the engine, so these are fixed now.

| Method and path | Action |
|---|---|
| `GET /api/state` | Full snapshot: camps, roads, queue, active list, log, AVL tree, route, stock, speed, operation trace |
| `POST /api/camps` | Add camp with chosen links |
| `PUT /api/camps/{id}` | Edit camp |
| `DELETE /api/camps/{id}` | Delete camp (`confirm=true` required if requests are pending) |
| `GET /api/camps/{id}` | Search |
| `POST /api/roads` | Add road |
| `PUT /api/roads` | Set status |
| `DELETE /api/roads` | Remove road |
| `POST /api/requests` | Submit (enqueue) |
| `PUT /api/requests/{id}` | Update an active request |
| `POST /api/process` | Process incoming (dequeue to list) |
| `POST /api/handle-next` | Serve the top request |
| `POST /api/settings` | Change average speed |
| `POST /api/scenario/{name}` | Load scenario |
| `POST /api/reset` | Clear everything |

Every mutating call returns the new full snapshot, so the browser just redraws.

---

## 12. Phases

Each phase ends with something you can run. Antigravity gets one written spec per phase. I write the spec, Antigravity implements and runs the tests, and I explain the result in plain terms before we move on.

### Phase 1: Engine part 1, requests (about 30 min)
**Build:** `Types.h`, Queue, RequestList (linked list A), DeliveryLog (linked list B), the priority comparison, hand-written merge sort, and tests.
**Done when:**
- Queue returns items in arrival order, and empty dequeue fails cleanly.
- List insert, search, update, delete (head, middle, tail) all work.
- Shuffled input comes out sorted, with ties in arrival order.
- The inversion case works: a large low-urgency request is ranked below a small critical one.
- Tests pass under the address sanitizer with no leaks.

### Phase 2: Engine part 2, camps and roads (about 50 min, the heaviest)
**Build:** AVLTree with delete, Graph, Dijkstra, the Engine facade, snapshots, scenario loading, a basic console menu, and tests.
**Done when:**
- All four rotation cases pass. Delete works for leaf, one child, two children and the root.
- After every insert and delete in a random sequence, the tree is ordered and every balance factor is within plus or minus 1.
- Dijkstra matches hand-solved fixtures: classic graph, blocked road changes the path, slow road keeps the path but a larger total, unreachable camp, equal-weight tie.
- Scenarios 1 to 4 load and give the expected results from the console menu.
- **If time runs out, stopping here still gives a graded-quality project.**

### Phase 3: Local server (about 25 min)
**Build:** `server/main.cpp` with the endpoints in section 11, static file serving for `web/`, input validation, JSON snapshots.
**Done when:** with `curl` or the browser, adding a camp returns an updated snapshot, a duplicate ID is rejected, and a scenario loads. The AVL snapshot shows heights and the operation trace names the rotation.

### Phase 4: Interface (about 50 min)
**Build:** the page in section 5: SVG map with click-to-place and click-to-cycle road status, camp table and forms, request form with the queue and ranked list, route highlighting, Data Structures panel with the AVL drawn as a tree, speed control, scenario dropdown.
**Done when:** the 5-minute viva script runs from start to finish with no errors.

**Total:** about 2 hours 35 minutes, so there is little slack.

### If time gets tight, cut in this order
1. Aging term and any stretch goals
2. Slow roads (keep only open and blocked)
3. Editing a camp's position
4. Stock and partial delivery
5. Interface polish (the plain page still works)
6. AVL deletion (insert and rotations are enough)
7. Phase 4 entirely, falling back to the console menu from Phase 2

---

## 13. Viva preparation

**Concept to file to function**

| Concept | File | Functions to open |
|---|---|---|
| Queue | `engine/Queue.cpp` | `enqueue`, `dequeue` |
| Linked list | `engine/RequestList.cpp` | `insert`, `deleteById`, `update` |
| Sorting | `engine/Sorting.cpp` | `mergeSort`, `merge`, comparison function |
| AVL tree | `engine/AVLTree.cpp` | `insert`, `rotateLeft`, `rotateRight`, `rebalance`, `remove` |
| Graph | `engine/Graph.cpp` | `addEdge`, `setStatus`, `computeTime` |
| Dijkstra | `engine/Dijkstra.cpp` | `run`, `relax`, `reconstructPath` |
| Whole flow | `engine/Engine.cpp` | `submitRequest`, `processIncoming`, `handleNext` |

**Complexities to state**

| Operation | Time |
|---|---|
| Queue enqueue / dequeue | O(1) |
| Merge sort of n requests | O(n log n) |
| AVL search, insert, delete | O(log n) |
| Dijkstra, array version | O(V squared) |
| Dijkstra, binary heap (upgrade) | O((V+E) log V) |

**Questions to expect, with short answers**

- *Where is the queue?* `Queue.cpp`. It holds arrival order. Service order comes from the sorted list.
- *Why not serve straight from the queue?* A FIFO queue ignores urgency, so the sort decides priority.
- *Why AVL, not a plain BST?* Sequential camp IDs turn a plain BST into a linked list. Scenario 4 shows this.
- *What if a road is blocked?* It is skipped, not given a big weight, and Dijkstra reruns.
- *What if a camp becomes unreachable?* It is reported as unreachable.
- *Can Dijkstra have negative weights?* No. The proof fails, so input is rejected.
- *Why not solve multi-stop routing?* That is the vehicle routing problem, which is NP-hard. This project does shortest path only.
- *Weakness of your priority rule?* Low urgency can starve. An aging term would fix it.

**What you must be able to do on paper:** draw the four AVL rotations, trace Dijkstra on a small graph, and trace merge sort on five items. The final evaluation includes an individual oral exam, so for every module Claude writes, a plain-language walkthrough and a hand-traced example go into `docs/VIVA_CHEATSHEET.md`.

---

## 14. Open decisions (defaults will be used unless you change them)

| # | Decision | Default |
|---|---|---|
| 1 | Priority rule | Lexicographic (section 7), not a weighted score |
| 2 | Stock and partial delivery | Included (section 8) |
| 3 | Deleting a camp with pending requests | Confirm, then cancel those requests |
| 4 | Incoming requests | Wait in the queue until **Process incoming** is clicked, so enqueue and dequeue are visible |
| 5 | STL use | None for the six required structures. Confirm with the course-in-charge. |
| 6 | Team split | Not yet known. The proposal lists five members and an individual oral exam. Decide who owns which module. |

---

## 15. Not doing

- AI or LLM components, real map tiles, live IDRN data, HDX or OSM datasets
- Multi-stop or multi-vehicle routing, vehicle capacity
- Multiple depots (stretch: a zero-weight super-source)
- One-way roads
- Playwright or browser-agent testing
- Deployment (Vercel or any hosting)
