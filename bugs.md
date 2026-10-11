# ReliefNet: bug report

**Reviewed:** branch `Graph,-Dijkstra,-Engine,`, first at commit `c6b70af`, then re-checked after
the Windows fix at `2a8373b` and again with BUG-7 fixed (2026-10-11).
**Platforms:** Linux (g++ 13.3, `-std=c++17 -Wall -Wextra`, AddressSanitizer + UBSan) and Windows
(`build_and_test.bat`, run by the author on the earlier fix).

## Summary

The core data structures (queue, request list, merge sort, AVL tree, graph, Dijkstra)
are correct. No bug was found in them. Every bug found was at the edges: input validation,
unchecked return values, the console, and one piece of stale state.

| ID | Severity | Status | Title |
|----|----------|--------|-------|
| BUG-1 | Medium | Fixed | Console silently dropped invalid link tokens when adding a camp |
| BUG-2 | Medium | Fixed | `Engine` ignored `addEdge` / `setEdgeDistance` failures (huge coordinates) |
| BUG-3 | Low | Fixed | Console roads view omitted travel minutes (spec requires them) |
| BUG-4 | Low | Fixed | `lastRotations()` was stale after `reset()` |
| BUG-5 | Low | Fixed | Camp received totals could overflow `int` (undefined behaviour) |
| BUG-6 | Cosmetic | Fixed | `deleteCamp(-5)` said "Cannot delete depot" |
| BUG-7 | Cosmetic | Fixed | Console input handling: trailing space, CRLF, `input.txt` |
| FIXED-1 | Medium | Fixed | NaN / infinity accepted as speed, coordinates, depot position |
| FIXED-2 | Medium | Fixed | `int` overflow in travel minutes and in Dijkstra's running total |
| FIXED-3 | Medium | Fixed | Scenario parser truncated large integers (**also needed a Windows fix**, see below) |
| FIXED-4 | Medium | Fixed | An out-of-stock request blocked every request behind it |
| FIXED-5 | Low | Fixed | `Scenario.cpp` used `isspace`/`strtod`/`strtol` without their headers |

---

## Open bugs

None.

---

## Fixed bugs

### BUG-1: Console silently dropped invalid link tokens

**Was:** `while (ss >> link)` in the add-camp menu stopped at the first token that was
not a number and ignored the rest. Entering `0 x 1` printed "Camp added" but only the
road to the depot existed.

**Fix:** the links line is read word by word and each word goes through the shared
`parseWholeNumber`. One bad word rejects the whole line with
"Links must be whole numbers separated by spaces. Camp not added." and the camp is not added.
Also rejects `1x`, `1.5`, `abc` and numbers too big for an `int`.

**Tests:** `parseWholeNumber` cases in `tests/test_types.cpp`. The console menu itself was
checked by running the real binary with scripted input, before and after.

### BUG-2: `Engine` ignored `addEdge` / `setEdgeDistance` failures

**Was:** a camp at a huge finite coordinate such as `1e200` has a squared distance of `1e400`,
which overflows to infinity. The graph correctly refused an infinite road, but `Engine` never
checked, so `addCamp` reported a road that did not exist, `addRoad` said "Road already exists"
for an invalid length, and `editCamp` moved a camp while leaving its roads at the old length.

**Fix (root cause):** coordinates are limited to plus or minus 1,000,000 km
(`kMaxCoordinateKm`, `isValidCoordinate` in `Types.h`). `isValidCamp`, `AVLTree::update`,
`editCamp` and `setDepotPosition` all use it, so no road length can overflow.

**Fix (all-or-nothing):** `addCamp`, `editCamp` and `setDepotPosition` now work out every road
length first and change nothing if any length is not finite or not above 0. This also catches
two different points so close together that the squared distance underflows to 0. The roads
actually created are counted, so `value` is true. `addRoad` now reports an invalid length as
`Invalid`, and "Road already exists" only for a real duplicate.

**Tests:** `tests/test_types.cpp`, `tests/test_avl.cpp`, `tests/test_engine.cpp` (blocks 18 to 20)
and `tests/test_scenarios.cpp`.

### BUG-3: Console roads view omitted travel minutes

**Fix:** menu 13, then f, now prints `a - b | km | status | minutes`
(for example `0 -   1 |   5.00 km | Open    | 8 min`). Verified by running the real binary.

### BUG-4: `lastRotations()` was stale after `reset()`

**Fix:** `AVLTree::clear()` now also runs `rotations.clear()`.

**Tests:** `tests/test_avl.cpp` and `tests/test_engine.cpp` (block 22).

### BUG-5: Camp received totals could overflow `int`

**Fix:** `AVLTree::addReceived` uses a saturating add, so a total stops at `INT_MAX`
instead of wrapping (signed overflow is undefined behaviour).

**Tests:** `tests/test_avl.cpp` and `tests/test_engine.cpp` (block 23), run under UBSan.

### BUG-6: Wrong message for negative camp ids

**Fix:** `deleteCamp(0)` still says "Cannot delete depot". A negative id now says
"Invalid camp id".

**Tests:** `tests/test_engine.cpp` (block 21).

### BUG-7: Console input handling

**Was:** `readInt` and `readDouble` required the line to end right after the number
(`ss.eof()`), so a trailing space (`13 `) was rejected as "Not a valid number". On Linux and
macOS an input file saved with CRLF line endings left a `\r` on every line and failed the same
way (Windows text mode hides the `\r`). And `input.txt` ended with `d` then `0`: the `0` was read
inside the data-structures sub-menu, which only leaves on a blank line, so the console printed
"Invalid option."

**Fix:** a small `trimLine` helper in `console/console_main.cpp` removes spaces, tabs and `\r` at
both ends of a line before it is parsed. `readInt`, `readDouble`, `readString` and the
data-structures sub-menu all use it. A line of only blanks still counts as blank. `input.txt`
now has the blank line before the final `0`. `engine/` is untouched.

**Tests:** `tests/console/console_input_test.sh` runs the real console binary with scripted
input (18 checks: trailing space, leading space, tab, CRLF, decimals, the sub-menu, bad numbers
still refused, and `input.txt` running to the end with no error). Run it with `make test-console`.
Against the old console 11 of the 18 fail; against the fixed one all pass.

---

## Fixed earlier on this branch

| ID | Commit | What changed |
|----|--------|--------------|
| FIXED-1 | `792eb8b` | `isfinite` checks in `isValidCamp`, `AVLTree::update`, `editCamp`, `setDepotPosition`, `Graph::addEdge`, `setEdgeDistance`, `setAverageSpeed`, and the scenario parser. `speed nan`, `speed inf` and `camp 1 A nan 4 10` are rejected. |
| FIXED-2 | `792eb8b` | `Graph::computeTime` compares as a `double` first and clamps to `kMaxRoadMinutes` (100000). Dijkstra cannot overflow for fewer than about 21,000 nodes. |
| FIXED-3 | `792eb8b`, `2a8373b` | See the Windows note below. |
| FIXED-4 | `ade3a3c` | `handleNext` skips a request whose resource is out of stock, the same way it skips unreachable camps. Spec updated to match. |
| FIXED-5 | `792eb8b` | Added `<cctype>`, `<cstdlib>`, `<cmath>`, `<climits>` to `Scenario.cpp`. |

### Note on FIXED-3: it was only fixed on Linux at first

The first fix range-checked `strtol`'s result. That works on Linux, where `long` is 64 bits. On
Windows `long` is 32 bits, so `strtol` silently saturates a huge value to `INT_MAX` and reports
the problem only through `errno`. The range check could never fire, and `camp 99999999999 ...`
was accepted as camp 2147483647. The author's Windows run caught this
(`test_scenarios.cpp:209`). The first review was done on Linux only, so it was missed.

Fixed in `2a8373b` with `strtoll`. The parser now lives in one place, `parseWholeNumber` in
`Types.cpp`, used by both the scenario loader and the console. It was tested under an emulation
of Windows `strtol` (32-bit, saturating) and the old version fails the same test line.

---

## Design notes (not bugs)

- **A failed scenario load wipes the current state.** `docs/PHASE_2B_SPEC.md` requires
  `reset()` on any scenario error, so a typo in a scenario file destroys the loaded
  session. Compliant with the spec, but worth knowing.
- **Scenario files are found relative to the working directory.** Run the console and
  the tests from the repo root (the Makefile and README state this).
- **Low-urgency starvation** (a known limitation already listed in `PROJECT_PLAN.md`).
- **Coordinate limit:** plus or minus 1,000,000 km. Far larger than any real map, but it is a
  deliberate limit, not a bug, so a coordinate beyond it is refused as invalid.

---

## How this was verified

- Built with `-Wall -Wextra -fsanitize=address,undefined`. Warnings: none.
- Test suite: **33,582 checks, 0 failures** on Linux with the sanitizers, and again under the
  Windows-style `strtol` emulation. The 85 new checks cover BUG-1 to BUG-6.
  Run `build_and_test.bat` on Windows to confirm the same count there.
- **Mutation check:** each fix was removed again in a scratch copy and the new tests were confirmed to
  fail (or UBSan to report) every time: BUG-2 in three places, BUG-4, BUG-5, BUG-6 and the
  Windows `strtol` problem.
- The console fixes (BUG-1, BUG-3) were checked by running the real binary with scripted input,
  before and after.
- Earlier, a separate fuzz harness (not committed) ran 150 random engine sessions of 250
  operations each. After every step it checked Dijkstra against a Bellman-Ford oracle,
  road lengths against camp positions, stock conservation, sorted order, FIFO order and
  AVL validity, with 0 failures and 0 sanitizer reports. That run was on the code before these fixes.
