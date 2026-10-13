# ReliefNet

A disaster-relief simulation engine in C++17. Camps, roads, requests and deliveries are all handled by hand-written data structures: a queue, linked lists, an AVL tree, a weighted graph, Dijkstra's algorithm and a merge sort. The engine does no input or output; the console menu and the web server both call it.

**Live demo:** https://relifnet.onrender.com (the page is behind a login, set by the host; see "Deploy" below). It runs on Render's free tier, so after a period of inactivity the first load can take about a minute while the service wakes up.

**Status:** Phases 1 to 4 are done (engine, four demo scenarios, console menu, tests, web server with its JSON API, and the browser page). The server is also deployed online with a Docker image and a login (see "Deploy"). See `docs/PROJECT_PLAN.md` for the plan and `docs/PHASE_*_SPEC.md` for each phase.

## Layout

| Folder | Contents |
|---|---|
| `engine/` | The data structures and the `Engine` class that ties them together |
| `console/` | The interactive console menu |
| `web/` | The browser page: `index.html`, `style.css`, `app.js`. Plain HTML, CSS and JavaScript, no downloads, works offline |
| `server/` | The web server: `main.cpp` (sockets, login, deployment settings), `Api.cpp` / `Api.h` (every endpoint), plus two single-header libraries |
| `Dockerfile` | Builds the server into a small image for hosting (used by Render) |
| `tests/` | One test file per module, plus `run_tests.cpp` |
| `tests/server/` | Tests of the API layer (`test_api.cpp`) and a curl script that checks a running server (`curl_smoke.sh`) |
| `tests/console/` | `console_input_test.sh`: runs the real console with scripted input to check how it reads a line (BUG-7) |
| `data/` | Four demo scenarios, each with a hand-checked `.expected.txt` |
| `docs/` | Project plan and per-phase specs |

## Build and run

Run every command from the repository root. The tests and the console read `data/` by relative path.

**Any platform with g++ and make:**

```sh
make test        # build and run the tests
make test-san    # the same, with AddressSanitizer and UBSan
make console     # build ./relief_console
make test-console   # run the console input-handling test (needs bash; on Windows, Git Bash)
make clean
```

**Windows without make:** run `build_and_test.bat` to build and run the tests, and `build_console.bat` to build `relief_console.exe`.

The web server is built and run as described under "Web server" below. Its tests are separate (`make test-server`, or `build_server_tests.bat`), so the original test suite does not need any downloaded file.

## Console

Start `relief_console` (or `relief_console.exe`). Menu item 1 loads one of the four scenarios, and item 13 shows the data structures: the queue, the ranked list, the delivery log, the AVL tree, the roads, the routes and the Dijkstra steps.

## Web server

`relief_server` serves a JSON API on `http://localhost:8080/` and the page in `web/`. By default it listens on the loopback address only, so other computers cannot reach it; to host it online, see "Deploy" below. The engine does the work; the server only reads the request, calls the engine and sends back the full state.

**The two libraries.** `server/` needs two single-header libraries: cpp-httplib (HTTP server) and nlohmann/json (JSON). Both are already committed in `server/` (`httplib.h` 0.18.7 and `json.hpp` 3.11.3), so a normal clone builds without any download. If either file is ever missing, fetch it again from the repository root in PowerShell:

```powershell
Invoke-WebRequest -Uri https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.7/httplib.h -OutFile server\httplib.h
Invoke-WebRequest -Uri https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp -OutFile server\json.hpp
```

(On Linux or macOS: `curl -L -o server/httplib.h <same URL>`.) Both are used for transport only. Confirm with your course-in-charge that they are allowed.

**Build and run, from the repository root** (the server reads `data/` and `web/` by relative path):

```sh
make serve               # builds ./relief_server and starts it
```

```bat
build_server.bat         rem Windows: builds relief_server.exe (needs g++ with POSIX threads, links -lws2_32)
relief_server.exe        rem optional port: relief_server.exe 9000
```

Then open `http://localhost:8080/` in a browser. If `web/` is missing, a placeholder page is shown and the API still works. The API can also be used directly, for example `curl http://localhost:8080/api/state` or `curl -X POST http://localhost:8080/api/scenario/scenario4_avl`. The console prints one line per API call. Stop it with Ctrl+C.

**The page.** It keeps no data of its own: each click is sent to the engine, and the full state that comes back is drawn.

- **Top bar:** scenario and Load, average speed and Set, Reset. Under it: depot stock (a resource at zero turns red) and the result of the last action (red when the engine refused it).
- **Left, camps:** a switch between the camp list and the add-camp form. The list shows each camp with its families, what it has received and a badge for its pending requests. You can search by ID, edit, or delete. Deleting a camp that has pending requests shows a confirmation bar in the panel before anything is cancelled.
- **Centre, map:** the map fills its pane. Click empty space to open the add-camp form with that position and a suggested road to the nearest node. Click a road to cycle open, slow, blocked (blocked roads are dashed). Click a camp to run an AVL search for it and highlight its route. Camps with pending requests carry an amber count. Under the map, **Road** has two dropdowns and the **Add road** and **Remove road** buttons; clicking a road on the map also selects it in the dropdowns, so removing it is one more click.
- **Right, requests:** Process incoming and Handle next, a collapsible submit form, the incoming queue, and the ranked list with the reason each row ranks above the next. Edit on a ranked row opens an inline form for urgency, families and quantity.
- **Bottom:** the operation trace and tabs for the queue and the active list (both drawn as linked lists), the delivery log, the AVL tree (heights, balance factors, last rotations, search path) and the Dijkstra steps and routes. **Hide panel** collapses the tabs and gives the map the room.
- **Keyboard:** camps and roads on the map and the camp cards can be reached with Tab and used with Enter or Space.

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

**Checks.** `make test-console` runs the console input-handling test (needs bash; on Windows run `build_console.bat`, then `bash tests/console/console_input_test.sh ./relief_console.exe` in Git Bash). `make test-server` (API layer, calls `Api::handle` directly, compares it against the engine on random sessions) and `make test-server-san` (the same with the sanitizers). With a server running, `bash tests/server/curl_smoke.sh 8080` exercises every endpoint over real HTTP; it starts with a reset, so it wipes the loaded state.

## Deploy

The live demo at https://relifnet.onrender.com is a Render **Web Service** that builds the `Dockerfile` in this repository from the `Graph,-Dijkstra,-Engine,` branch. The image compiles the server with g++ 13 and links `libstdc++` statically, so it runs on the small `debian:bookworm-slim` base.

The server reads these settings from environment variables. Set them in the host's dashboard (on Render: the service's **Environment** tab), never in the code or the repository:

| Variable | Needed | Meaning |
|---|---|---|
| `APP_USER` | Yes when online | Username for the HTTP Basic login that protects every page and API call |
| `APP_PASS` | Yes when online | Password for that login |
| `BIND_HOST` | Set by the Dockerfile | Address to listen on. Default `127.0.0.1` (local only); the image sets `0.0.0.0` |
| `PORT` | Set by the host | Port to listen on when no port argument is given. Default 8080 |

The server refuses to start on a public address (`BIND_HOST` other than `127.0.0.1` or `localhost`) unless both `APP_USER` and `APP_PASS` are set, so it can never be exposed without a login. If a deploy exits at once with "Refusing to listen ... Set APP_USER and APP_PASS", those two variables are missing.

To try the image on your own machine (needs Docker):

```sh
docker build -t relifnet .
docker run --rm -p 8080:10000 -e PORT=10000 -e APP_USER=demo -e APP_PASS=change-me relifnet
```

Then open `http://localhost:8080/` and log in with the username and password you passed.

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
