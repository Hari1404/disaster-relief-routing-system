# ReliefNet: bug report

**Reviewed:** branch `Graph,-Dijkstra,-Engine,` at commit `c6b70af` (2026-10-11).
**Platform:** Linux, g++ 13.3, `-std=c++17 -Wall -Wextra`, AddressSanitizer + UBSan.

## Summary

The core data structures (queue, request list, merge sort, AVL tree, graph, Dijkstra)
are correct. No bug was found in them. The open bugs are all at the edges: input
validation, unchecked return values, the console, and one piece of stale state.

| ID | Severity | Status | Title |
|----|----------|--------|-------|
| BUG-1 | Medium | **Open** | Console silently drops invalid link tokens when adding a camp |
| BUG-2 | Medium | **Open** | `Engine` ignores `addEdge` / `setEdgeDistance` failures (huge coordinates) |
| BUG-3 | Low | **Open** | Console roads view omits travel minutes (spec requires them) |
| BUG-4 | Low | **Open** | `lastRotations()` is stale after `reset()` |
| BUG-5 | Low | **Open** | Camp received totals can overflow `int` (undefined behaviour) |
| BUG-6 | Cosmetic | **Open** | `deleteCamp(-5)` says "Cannot delete depot" |
| BUG-7 | Cosmetic | **Open** | Console input handling: trailing space, CRLF, `input.txt` |
| FIXED-1 | Medium | Fixed | NaN / infinity accepted as speed, coordinates, depot position |
| FIXED-2 | Medium | Fixed | `int` overflow in travel minutes and in Dijkstra's running total |
| FIXED-3 | Medium | Fixed | Scenario parser truncated large integers (`strtol` to `int`) |
| FIXED-4 | Medium | Fixed | An out-of-stock request blocked every request behind it |
| FIXED-5 | Low | Fixed | `Scenario.cpp` used `isspace`/`strtod`/`strtol` without their headers |

---

## Open bugs

### BUG-1 (Medium): Console silently drops invalid link tokens

**Where:** `console/console_main.cpp:119`, `while (ss >> link)`.

**What happens:** The loop stops at the first token that is not a number and ignores
the rest. The camp is still added and the console says "Camp added".

**Repro (menu 2, Links prompt):** add camp 1 at (3,4), then add camp 2 with links
`0 x 1`. Camp 2 gets a road to the depot only. The link to camp 1 is lost with no
warning. Menu 13, then f, shows only `0 - 2`.

**Fix:** after the loop, check that the whole line was consumed (`ss.eof()`, not
`ss.fail()`). If not, print "Links must be numbers" and return to the menu without
calling `addCamp`.

### BUG-2 (Medium): `Engine` ignores `addEdge` / `setEdgeDistance` failures

**Where:** `engine/Engine.cpp` lines 97 (`addCamp`), 153 (`editCamp`), 224
(`setDepotPosition`) and 241 (`addRoad`).

**Background:** The finite-number fix (FIXED-1) made `Graph::addEdge` and
`Graph::setEdgeDistance` refuse an infinite distance. That is correct, but a camp at
a huge finite coordinate such as `1e200` has a squared distance of `1e400`, which
overflows to infinity inside `distanceBetween`. `Engine` never checks the return
values, so three things go wrong silently:

| Case | Repro | Result |
|------|-------|--------|
| A. `addCamp` | `Camp{1,"far",1e200,0,10,0,0,0}` linked to `{0}` | Returns ok, `value` = 1 road created. Actually 0 roads exist and camp 1 is unreachable. |
| B. `addRoad` | two camps at x = +1e200 and -1e200, then `addRoad(1,2)` | Returns `Duplicate`, "Road already exists". The real reason is an invalid distance. |
| C. `editCamp` | move linked camp 1 from (3,4) to (1e200, 0) | Returns ok. The road still says 5 km / 8 min while the camp is 1e200 km away (stale data). |

**Fix (root cause):** bound coordinates, for example `|x|, |y| <= 1e6` km, in
`isValidCamp`, `AVLTree::update`, `Engine::editCamp` and `Engine::setDepotPosition`.
Then no distance can overflow. Also check the return value of `addEdge` /
`setEdgeDistance` and return `Err::Invalid` instead of dropping the failure.

### BUG-3 (Low): Console roads view omits travel minutes

**Where:** `console/console_main.cpp:355`.

**What happens:** Menu 13, then f, prints `a - b | km | status`. The spec
(`docs/PHASE_2B_SPEC.md`, console section) says the roads view shows
`a, b, km, status, minutes`. `EdgeInfo::minutes` is already available.

**Fix:** add `r.minutes` to the `printf` and a `%d min` column.

### BUG-4 (Low): `lastRotations()` is stale after `reset()`

**Where:** `engine/AVLTree.cpp:24`, `AVLTree::clear()`.

**What happens:** `clear()` resets the root and the count but not `rotations`. After
adding camps 10, 20, 30 (one RR rotation) and calling `Engine::reset()`,
`lastRotations()` still returns that rotation while `campCount()` is 0.

**Fix:** add `rotations.clear();` to `AVLTree::clear()`.

### BUG-5 (Low): Camp received totals can overflow `int`

**Where:** `engine/AVLTree.cpp:300` (and the Water/Medicine lines below it),
`receivedFood += amount`. `Engine::setStock` has no upper bound.

**What happens:** With stock and request quantity both `INT_MAX`, two deliveries
overflow the camp total. UBSan reports `signed integer overflow`, and the total reads
back as `-2`. Contrived, but it is undefined behaviour.

**Fix:** cap the total (saturating add), or reject a `setStock` above a sensible
limit.

### BUG-6 (Cosmetic): Wrong message for negative camp ids

**Where:** `engine/Engine.cpp:163`.

`deleteCamp(-5, false)` returns "Cannot delete depot". Only id 0 is the depot. Use
"Invalid camp id" for negative ids.

### BUG-7 (Cosmetic): Console input handling

**Where:** `console/console_main.cpp` (`readInt`, `readDouble`) and `input.txt`.

- A trailing space (`13 `) is rejected as "Not a valid number", because the parse
  requires `ss.eof()`.
- On Linux and macOS, an input file with CRLF line endings fails for the same reason
  (`\r` is left in the line).
- `input.txt` ends with `d` then `0`. The `0` is read inside the data-structures
  sub-menu, which only exits on a blank line, so the console prints "Invalid option."
  Add a blank line before the final `0`.

**Fix:** trim whitespace and `\r` from each line before parsing.

---

## Fixed on this branch (verified)

| ID | Commit | What changed |
|----|--------|--------------|
| FIXED-1 | `792eb8b` | `isfinite` checks in `isValidCamp`, `AVLTree::update`, `editCamp`, `setDepotPosition`, `Graph::addEdge`, `setEdgeDistance`, `setAverageSpeed`, and the scenario parser. `speed nan`, `speed inf` and `camp 1 A nan 4 10` are now rejected. |
| FIXED-2 | `792eb8b` | `Graph::computeTime` compares as a `double` first and clamps to `kMaxRoadMinutes` (100000). A 1e12 km road is 100000 minutes instead of 1,056,413,696, and Dijkstra can no longer overflow for fewer than about 21,000 nodes. |
| FIXED-3 | `792eb8b` | `parseInt` range-checks the `strtol` result. `camp 4294967297 ...` is rejected instead of becoming camp 1. |
| FIXED-4 | `ade3a3c` | `handleNext` skips a request whose resource is out of stock, the same way it already skipped unreachable camps. Spec updated to match. |
| FIXED-5 | `792eb8b` | Added `<cctype>`, `<cstdlib>`, `<cmath>`, `<climits>` to `Scenario.cpp`. |

---

## Design notes (not bugs)

- **A failed scenario load wipes the current state.** `docs/PHASE_2B_SPEC.md` requires
  `reset()` on any scenario error, so a typo in a scenario file destroys the loaded
  session. Compliant with the spec, but worth knowing.
- **Scenario files are found relative to the working directory.** Run the console and
  the tests from the repo root (the new Makefile and README state this).
- **Low-urgency starvation** (a known limitation already listed in `PROJECT_PLAN.md`).

---

## How this was verified

- Built with `-Wall -Wextra -fsanitize=address,undefined`. Warnings: none.
- Existing suite: **33,494 checks, 0 failures**. None of the existing tests exercise
  BUG-1 to BUG-7.
- A separate fuzz harness (not committed) ran 150 random engine sessions of 250
  operations each. After every step it checked Dijkstra against a Bellman-Ford oracle,
  road lengths against camp positions, stock conservation, sorted order, FIFO order and
  AVL validity. It also fuzzed the AVL tree, queue, list and merge sort against
  standard-library oracles. **0 failures, 0 sanitizer reports.**
- The harness was mutation-tested: two deliberately planted bugs (stale road distance
  after `editCamp`, queue tail not fixed in `removeByCamp`) were both caught.
- Each open bug above was reproduced on this branch with a small probe program or the
  real console binary.
