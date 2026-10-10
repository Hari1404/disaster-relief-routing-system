# ReliefNet

A disaster-relief simulation engine in C++17. Camps, roads, requests and deliveries are all handled by hand-written data structures: a queue, linked lists, an AVL tree, a weighted graph, Dijkstra's algorithm and a merge sort. The engine does no input or output; the console menu (and, later, a web server) call it.

**Status:** Phases 1 to 2B are done (engine, four demo scenarios, console menu, tests). The local server and web interface (Phases 3 and 4) are not built yet. See `docs/PROJECT_PLAN.md` for the plan and `docs/PHASE_*_SPEC.md` for each phase.

## Layout

| Folder | Contents |
|---|---|
| `engine/` | The data structures and the `Engine` class that ties them together |
| `console/` | The interactive console menu |
| `tests/` | One test file per module, plus `run_tests.cpp` |
| `data/` | Four demo scenarios, each with a hand-checked `.expected.txt` |
| `docs/` | Project plan and per-phase specs |

## Build and run

Run every command from the repository root. The tests and the console read `data/` by relative path.

**Any platform with g++ and make:**

```sh
make test        # build and run the tests
make test-san    # the same, with AddressSanitizer and UBSan
make console     # build ./relief_console
make clean
```

**Windows without make:** run `build_and_test.bat` to build and run the tests, and `build_console.bat` to build `relief_console.exe`.

## Console

Start `relief_console` (or `relief_console.exe`). Menu item 1 loads one of the four scenarios, and item 13 shows the data structures: the queue, the ranked list, the delivery log, the AVL tree, the roads, the routes and the Dijkstra steps.

## Demo scenarios

| # | File | What it shows |
|---|---|---|
| 1 | `data/scenario1_normal.txt` | One depot, six camps, ten roads, six requests, ranked and served |
| 2 | `data/scenario2_blockage.txt` | Blocking roads changes a route, then makes a camp unreachable |
| 3 | `data/scenario3_priority.txt` | A small critical request beats a big low-urgency one; stock runs short and one delivery is partial |
| 4 | `data/scenario4_avl.txt` | Camp IDs inserted in order 10, 20, 30, 40, 50, 25 cause three AVL rotations |

## Scenario file format

Plain text, one command per line. Blank lines and lines starting with `#` are ignored. Camp names are a single word. Camps must appear before any road or request that uses them. Node 0 is the depot.

```
depot X Y                                      depot position, in km
speed KMH                                      average speed
stock FOOD WATER MEDICINE                      depot stock
camp ID NAME X Y FAMILIES                      a camp (ID 1 or more)
road A B                                       a road; its length comes from the two positions
request CAMP RESOURCE QUANTITY URGENCY FAMILIES
```

`RESOURCE` is `food`, `water` or `medicine`. `URGENCY` runs from 1 (low) to 4 (critical). A bad line stops the load with a message like `line 7: unknown command`, and the engine is reset to empty. Values such as `nan` and `inf` are rejected.

## Rules worth knowing

- **Priority order:** urgency (higher first), then families affected, then quantity, then arrival order (earlier first).
- **Handle next:** serves the first request in ranked order whose camp is reachable and whose resource is in stock. A request that fails either test is skipped and stays in the active list. A partial delivery leaves the remainder in the list.
- **Travel time:** whole minutes, at least 1 and at most `Graph::kMaxRoadMinutes` (100000) per road. Slow roads run at half speed. Blocked roads are never used.
- **Dijkstra:** the array version, O(V squared). Route totals are `int`, which is safe for fewer than about 21,000 nodes.
- **Camp IDs** start at 1. ID 0 is the depot.
