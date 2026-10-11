# ReliefNet

A disaster-relief simulation engine in C++17. Camps, roads, requests and deliveries are all handled by hand-written data structures: a queue, linked lists, an AVL tree, a weighted graph, Dijkstra's algorithm and a merge sort. The engine does no input or output; the console menu (and, later, a web server) call it.

**Status:** Phases 1 to 4 are done (engine, four demo scenarios, console menu, tests, local web server with its JSON API, and the browser page). See `docs/PROJECT_PLAN.md` for the plan and `docs/PHASE_*_SPEC.md` for each phase.

## Layout

| Folder | Contents |
|---|---|
| `engine/` | The data structures and the `Engine` class that ties them together |
| `console/` | The interactive console menu |
| `web/` | The browser page: `index.html`, `style.css`, `app.js`. Plain HTML, CSS and JavaScript, no downloads, works offline |
| `server/` | The local web server: `main.cpp` (sockets), `Api.cpp` / `Api.h` (every endpoint), plus two downloaded single headers |
| `tests/` | One test file per module, plus `run_tests.cpp` |
| `tests/server/` | Tests of the API layer (`test_api.cpp`) and a curl script that checks a running server (`curl_smoke.sh`) |
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

The web server is built and run as described under "Web server" below. Its tests are separate (`make test-server`, or `build_server_tests.bat`), so the original test suite does not need any downloaded file.

## Console

Start `relief_console` (or `relief_console.exe`). Menu item 1 loads one of the four scenarios, and item 13 shows the data structures: the queue, the ranked list, the delivery log, the AVL tree, the roads, the routes and the Dijkstra steps.

## Web server

`relief_server` serves a JSON API on `http://localhost:8080/` and the page in `web/`. It listens on the loopback address only, so other computers cannot reach it. The engine does the work; the server only reads the request, calls the engine and sends back the full state.

**One-time download.** Two single-header libraries are needed in `server/`: cpp-httplib (HTTP server) and nlohmann/json (JSON). They are not part of the patch. From the repository root in PowerShell:

```powershell
Invoke-WebRequest -Uri https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.7/httplib.h -OutFile server\httplib.h
Invoke-WebRequest -Uri https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp -OutFile server\json.hpp
```

(On Linux or macOS: `curl -L -o server/httplib.h <same URL>`.) `git status` will show them as new files; commit them if you want a fresh clone to build without the download. Both are used for transport only. Confirm with your course-in-charge that they are allowed.

**Build and run, from the repository root** (the server reads `data/` and `web/` by relative path):

```sh
make serve               # builds ./relief_server and starts it
```

```bat
build_server.bat         rem Windows: builds relief_server.exe (needs g++ with POSIX threads, links -lws2_32)
relief_server.exe        rem optional port: relief_server.exe 9000
```

Then open `http://localhost:8080/` in a browser. If `web/` is missing, a placeholder page is shown and the API still works. The API can also be used directly, for example `curl http://localhost:8080/api/state` or `curl -X POST http://localhost:8080/api/scenario/scenario4_avl`. The console prints one line per API call. Stop it with Ctrl+C.

**The page.** It keeps no data of its own: each click is sent to the engine, and the full state that comes back is drawn. Top bar: scenario, average speed, reset, depot stock, and the result of the last action. Left: camps table (select, edit, delete, search by ID) and the add-camp form. Centre: the map. Click empty space to fill the add-camp form with that position (and a suggested road to the nearest node), click a road to cycle open, slow, blocked (blocked roads are dashed, the route to the selected camp is highlighted), click a camp to run an AVL search for it. Right: submit a request, Process incoming, Handle next, the incoming queue, and the ranked list with the reason each row ranks above the next. Bottom: the operation trace and tabs for the queue, the active list (both drawn as linked lists), the delivery log, the AVL tree (heights, balance factors, last rotations, search path) and the Dijkstra steps and routes. Deleting a camp that has pending requests asks for confirmation first.

**Endpoints.** Every reply is JSON: `ok`, `error` (null on success), `message`, any extra fields, and `state`, the full snapshot. A call that changes something returns the new state, and so does a refused one, so the page can always redraw.

| Method and path | Body or query | Action |
|---|---|---|
| `GET /api/state` | | Full snapshot: camps, roads, routes, queue, ranked active list (with each row's rank reason), delivery log, AVL tree (heights, balance factors, last rotations), Dijkstra steps, stock, speed, operation trace |
| `POST /api/camps` | `id, name, x, y, families, links?` | Add a camp, with roads to the camps in `links` (0 is the depot) |
| `GET /api/camps/{id}` | | AVL search: the camp, the ids visited from the root, its route |
| `PUT /api/camps/{id}` | `name, x, y, families` | Edit a camp |
| `DELETE /api/camps/{id}` | `?confirm=true` | Delete a camp. Refused (409) while requests are pending, unless confirmed |
| `POST /api/roads` | `a, b` | Add a road |
| `PUT /api/roads` | `a, b, status` | Set `open`, `slow` or `blocked` |
| `DELETE /api/roads` | `a, b` | Remove a road |
| `POST /api/requests` | `campId, resource, quantity, urgency, families` | Submit (enqueue) |
| `PUT /api/requests/{id}` | `urgency, families, quantity` | Update an active request |
| `POST /api/process` | | Process incoming (queue to ranked list) |
| `POST /api/handle-next` | | Serve the top servable request |
| `POST /api/settings` | `speedKmh` | Change the average speed |
| `POST /api/scenario/{name}` | | Load `scenario1_normal`, `scenario2_blockage`, `scenario3_priority` or `scenario4_avl` |
| `POST /api/reset` | | Clear everything |

**Status codes.** 200 done. 400 bad input (wrong type, missing or unknown field, out of range). 404 camp, request, road or scenario not found. 409 valid request the current state refuses: duplicate id or road, delete needs confirmation, nothing to process or handle, unreachable camp, out of stock. 403 a request carrying another web page's `Origin` header. 405 wrong method. 413 body over 64 KB.

**Checks.** `make test-server` (API layer, calls `Api::handle` directly, compares it against the engine on random sessions) and `make test-server-san` (the same with the sanitizers). With a server running, `bash tests/server/curl_smoke.sh 8080` exercises every endpoint over real HTTP; it starts with a reset, so it wipes the loaded state.

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

`RESOURCE` is `food`, `water` or `medicine`. `URGENCY` runs from 1 (low) to 4 (critical). A bad line stops the load with a message like `line 7: unknown command`, and the engine is reset to empty. Values such as `nan` and `inf` are rejected, and so is any coordinate outside plus or minus 1,000,000 km.

## Rules worth knowing

- **Priority order:** urgency (higher first), then families affected, then quantity, then arrival order (earlier first).
- **Handle next:** serves the first request in ranked order whose camp is reachable and whose resource is in stock. A request that fails either test is skipped and stays in the active list. A partial delivery leaves the remainder in the list.
- **Travel time:** whole minutes, at least 1 and at most `Graph::kMaxRoadMinutes` (100000) per road. Slow roads run at half speed. Blocked roads are never used.
- **Dijkstra:** the array version, O(V squared). Route totals are `int`, which is safe for fewer than about 21,000 nodes.
- **Coordinates** (camps and depot) must be within plus or minus 1,000,000 km, so no road length can overflow. Anything else is refused as invalid.
- **Camp IDs** start at 1. ID 0 is the depot.
