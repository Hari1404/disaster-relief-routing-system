# Phase 1 Spec: Requests Engine (queue, linked lists, sorting)

**Project:** ReliefNet, a C++ disaster-relief simulation (college data-structures micro-project)
**This phase:** the request-handling half of the engine. No AVL tree, no graph, no Dijkstra, no server, no UI.
**Time budget:** about 30 minutes

> **Paste this into Antigravity:**
> "Read `docs/PHASE_1_SPEC.md` and implement it exactly. Follow the file names, function names and signatures as written. Do not add features, rename anything or touch anything outside this spec. When finished, run the tests and report the results as described in section 10. If anything in the spec is unclear or conflicts, stop and ask instead of guessing."

---

## 1. Why this matters (context)

This code will be explained line by line in an oral exam by a student who did not type it. So:

- Keep the code **simple and readable**. Prefer clarity over cleverness.
- Every public function gets a **one or two line comment** saying what it does and its time complexity.
- Every pointer manipulation step in the linked structures gets a short comment.
- The structures must be **real, hand-written linked structures**. They must not wrap a standard container.

---

## 2. Hard rules

1. Language: C++17. Compile with `g++`.
2. **Do not use** `std::queue`, `std::list`, `std::forward_list`, `std::priority_queue`, `std::deque`, `std::map`, `std::set`, or `std::sort` in anything under `engine/`.
3. `std::vector` and `std::string` are allowed **only** as output buffers (for example `toVector()`) and in test code.
4. `std::sort` is allowed **only in tests**, as an oracle to check the hand-written merge sort.
5. No external libraries and no frameworks. Tests use the tiny helper in section 7.
6. No global variables in `engine/`.
7. No `using namespace std;` anywhere in headers.
8. Every class that owns nodes has a destructor that frees all of them. Copying is disabled (`= delete` for copy constructor and copy assignment).
9. Do not create files that are not listed in section 3.

---

## 3. Files to create

The **repository root is the project root**. Create these folders directly in the repo root. Do **not** create an extra `reliefnet/` folder inside it.

```
(repo root)/
  engine/
    Types.h          Types.cpp
    Queue.h          Queue.cpp
    RequestList.h    RequestList.cpp
    DeliveryLog.h    DeliveryLog.cpp
    Sorting.h        Sorting.cpp
  tests/
    test_helpers.h
    test_queue.cpp
    test_request_list.cpp
    test_sorting.cpp
    test_delivery_log.cpp
    test_types.cpp
    run_tests.cpp
  build_and_test.bat
  .gitignore         (ignore build outputs and run_tests.exe)
  README.md          (short: what this is, how to build and run tests on Windows)
  docs/              (already contains PHASE_1_SPEC.md and PROJECT_PLAN.md; leave them)
```

---

## 4. Types (`engine/Types.h`, `engine/Types.cpp`)

```cpp
enum class ResourceType { Food = 0, Water = 1, Medicine = 2 };

struct Request {
    int          id;          // unique request ID, assigned by the caller
    int          campId;      // must be >= 1 (0 is reserved for the depot)
    ResourceType resource;
    int          quantity;    // units requested, > 0
    int          urgency;     // 1 = Low, 2 = Medium, 3 = High, 4 = Critical
    int          families;    // families affected, > 0
    long         arrivalSeq;  // arrival order number, >= 1, assigned by the caller
};

// One singly linked node holding a Request. Used by Queue and RequestList.
struct RequestNode {
    Request      data;
    RequestNode* next;
};

struct DeliveryRecord {
    int              requestId;
    int              campId;
    ResourceType     resource;
    int              quantityRequested;
    int              quantityDelivered;
    int              totalMinutes;   // route time in whole minutes
    std::vector<int> path;           // camp IDs from depot to camp, e.g. 0, 3, 5
};

// Returns true only if every field is within the ranges stated above.
bool isValidRequest(const Request& r);
```

`isValidRequest` checks: `id > 0`, `campId >= 1`, `quantity > 0`, `families > 0`, `urgency` in 1..4, `arrivalSeq >= 1`, and `resource` is one of the three enum values.

The caller (the Engine, built in Phase 2) will own the counters that create `id` and `arrivalSeq`. In this phase, tests set them by hand.

---

## 5. The priority rule and merge sort (`engine/Sorting.h`, `engine/Sorting.cpp`)

**Ordering rule.** Request `a` comes before request `b` when, comparing in this order:

1. higher `urgency`
2. more `families`
3. larger `quantity`
4. smaller `arrivalSeq` (earlier arrival)

Because `arrivalSeq` is unique, this is a strict total order.

```cpp
// True if a should be served before b. Strict: higherPriority(a, a) is false.
bool higherPriority(const Request& a, const Request& b);

enum class RankKey { Urgency, Families, Quantity, Arrival };

// The first key (in the order above) on which a and b differ.
// If they are identical on all four keys, return Arrival.
RankKey rankReason(const Request& a, const Request& b);

// "urgency", "families", "quantity", "arrival"
const char* rankKeyName(RankKey k);

// Hand-written, stable merge sort on a singly linked list of RequestNode.
// Returns the new head. Relinks the existing nodes; allocates no new nodes.
// O(n log n) time.
RequestNode* mergeSort(RequestNode* head);

// Merge two already-sorted lists into one sorted list.
// When neither node is higher priority than the other, take from the first list (stability).
RequestNode* merge(RequestNode* a, RequestNode* b);

// Split a list into two halves using the slow/fast pointer method.
// Returns the head of the second half; the first half ends with next = nullptr.
RequestNode* splitList(RequestNode* head);
```

Rules:
- `mergeSort` of an empty list or a one-node list returns it unchanged.
- Use recursion for `mergeSort` (depth is only log n).
- `merge` may be written iteratively with a dummy head node, or recursively. Either is fine. Comment the approach.

---

## 6. Containers

### 6.1 Queue (`engine/Queue.h`, `engine/Queue.cpp`)

A FIFO queue of `Request`, built from `RequestNode` with **head and tail pointers** and a size counter.

```cpp
class Queue {
public:
    Queue();
    ~Queue();
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    void enqueue(const Request& r);          // add at the back, O(1)
    bool dequeue(Request& out);              // remove from the front into out; false if empty, O(1)
    bool peek(Request& out) const;           // copy the front without removing; false if empty
    bool isEmpty() const;
    int  size() const;
    int  removeByCamp(int campId);           // remove all requests for a camp; returns how many, O(n)
    std::vector<Request> toVector() const;   // front to back
    void clear();                            // free all nodes
};
```

`removeByCamp` must keep `head`, `tail` and `size` correct in every case: removing the head, a middle node, the tail, several nodes in a row, or all nodes.

### 6.2 RequestList (`engine/RequestList.h`, `engine/RequestList.cpp`)

A singly linked list of active requests with **head and tail pointers** and a size counter.

```cpp
class RequestList {
public:
    RequestList();
    ~RequestList();
    RequestList(const RequestList&) = delete;
    RequestList& operator=(const RequestList&) = delete;

    bool insert(const Request& r);      // append at the tail; false (and no change) if r.id already exists
    bool searchById(int id, Request& out) const;                 // linear search
    int  searchByCamp(int campId, std::vector<Request>& out) const;  // appends matches in list order; returns count
    bool update(int id, int urgency, int families, int quantity);
                                        // false if id not found OR any new value is invalid
                                        // (urgency outside 1..4, families <= 0, quantity <= 0); on false nothing changes
    bool deleteById(int id);            // false if not found
    int  deleteByCamp(int campId);      // returns how many were removed
    bool getTop(Request& out) const;    // copy of the head; false if empty
    void sortByPriority();              // calls mergeSort, then re-finds the tail
    bool isSortedByPriority() const;    // true if every neighbouring pair satisfies higherPriority(prev, next)
    bool isEmpty() const;
    int  size() const;
    std::vector<Request> toVector() const;   // head to tail
    void clear();
};
```

Important design points:
- `insert` and `update` **never sort**. Only `sortByPriority()` sorts. The Engine will call it after changes. (`isSortedByPriority()` lets the Engine and tests check this.)
- `sortByPriority` must leave `tail` pointing at the real last node.
- `deleteById` must handle: only element, head, middle, tail.
- After deleting the tail, a following `insert` must still append correctly.

### 6.3 DeliveryLog (`engine/DeliveryLog.h`, `engine/DeliveryLog.cpp`)

A singly linked list of `DeliveryRecord` with head and tail pointers. Records only get added at the end.

```cpp
class DeliveryLog {
public:
    DeliveryLog();
    ~DeliveryLog();
    DeliveryLog(const DeliveryLog&) = delete;
    DeliveryLog& operator=(const DeliveryLog&) = delete;

    void append(const DeliveryRecord& rec);                      // O(1)
    int  size() const;
    bool isEmpty() const;
    std::vector<DeliveryRecord> toVector() const;                // oldest to newest
    std::vector<DeliveryRecord> searchByCamp(int campId) const;  // oldest to newest
    void clear();
};
```

Use its own node type (for example `DeliveryNode`) defined privately in `DeliveryLog.h`.

---

## 7. Test helper (`tests/test_helpers.h`)

```cpp
#pragma once
#include <cstdio>
extern int g_checks;
extern int g_failures;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; \
    std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
```

`run_tests.cpp` defines `g_checks` and `g_failures`, calls each suite function, prints a section heading before each, prints `N checks, M failures` at the end, and **returns non-zero if there are any failures**.

**Leak check (works on Windows with no sanitizers).** `run_tests.cpp` also replaces the global `operator new` and `operator delete` (all the standard forms, including the array and sized-delete versions) with versions that count allocations and frees. Use `std::malloc` and `std::free` inside, and throw `std::bad_alloc` if `malloc` returns null. Around each suite call, record the counters before and after. When the suite finishes, the number of allocations and frees made during that suite must be equal. If they differ, record a failure that names the suite. This code lives only in `run_tests.cpp` and never in `engine/`.

Each test file exposes one function: `runTypesTests()`, `runQueueTests()`, `runRequestListTests()`, `runSortingTests()`, `runDeliveryLogTests()`.

Add a small test-only helper `makeRequest(id, camp, resource, qty, urgency, families, seq)` in `test_helpers.h` to build requests quickly.

---

## 8. Tests that must exist

### 8.1 Types
- A valid request passes.
- Each field failing alone makes `isValidRequest` false: `id = 0`, `campId = 0`, `quantity = 0`, `families = 0`, `urgency = 0`, `urgency = 5`, `arrivalSeq = 0`.
- An out-of-range resource value (cast from an int such as 9) fails.

### 8.2 Queue
1. A new queue is empty, size 0; `dequeue` and `peek` return false.
2. Enqueue three requests: size is 3, and dequeue returns them in arrival order, then the queue is empty again.
3. `peek` returns the front and does not remove it.
4. Interleaved: enqueue A, B; dequeue gives A; enqueue C; dequeue gives B, then C.
5. `removeByCamp`: remove from the front, from the middle, from the back, several in a row, all of them, and a camp with no requests (returns 0). After each, `size()` and `toVector()` are correct.
6. After `removeByCamp` removes the last node, a following `enqueue` still works and ends up at the back (tail correct).
7. `toVector` returns front-to-back order.
8. `clear` empties the queue and it can be reused.
9. Stress: enqueue 1000 requests and dequeue all 1000 in the correct order.

### 8.3 RequestList
1. Insert three; `size()` is 3; `searchById` finds each and misses an unknown ID.
2. Inserting a duplicate ID returns false and leaves the size unchanged.
3. `searchByCamp` returns all matches in list order and returns the count; returns 0 for none.
4. `update` changes the three fields; the other fields stay the same.
5. `update` rejects `urgency = 0`, `urgency = 5`, `families = 0`, `quantity = 0`, and an unknown ID, and leaves the record unchanged in every case.
6. `deleteById`: only element, head, middle, tail, and a missing ID.
7. After deleting the tail, `insert` appends correctly.
8. `deleteByCamp`: returns the right count for some, all, and none.
9. `getTop` is false on an empty list; true after inserts (returns the head).
10. `clear` empties and the list is reusable.

### 8.4 Sorting and priority
1. `higherPriority`, one test per key: higher urgency wins; with equal urgency more families wins; with equal urgency and families larger quantity wins; with all three equal the earlier `arrivalSeq` wins.
2. `higherPriority(a, a)` is false. For two different requests, exactly one of `higherPriority(a, b)` and `higherPriority(b, a)` is true.
3. **Inversion case.** A = urgency 1, families 500, quantity 900, seq 1. B = urgency 4, families 10, quantity 5, seq 2. B must come first.
4. **Hand-checked fixture** (the viva example). Six requests, all with `campId` as shown:

| id | camp | resource | quantity | urgency | families | seq |
|---|---|---|---|---|---|---|
| 1 | 1 | Food | 50 | 2 | 30 | 1 |
| 2 | 2 | Water | 200 | 4 | 10 | 2 |
| 3 | 3 | Medicine | 20 | 4 | 40 | 3 |
| 4 | 1 | Food | 80 | 2 | 30 | 4 |
| 5 | 4 | Water | 500 | 1 | 200 | 5 |
| 6 | 2 | Medicine | 20 | 4 | 40 | 6 |

   Insert in the order 1, 2, 3, 4, 5, 6, call `sortByPriority`, and check the resulting ID order is exactly **3, 6, 2, 4, 1, 5**.
   Then clear, insert in the order **5, 2, 6, 1, 3, 4**, sort again, and check the same result **3, 6, 2, 4, 1, 5**.
   Check `rankReason` for the five neighbouring pairs in that order (3,6), (6,2), (2,4), (4,1), (1,5): the results must be **arrival, families, urgency, quantity, urgency**.
5. `sortByPriority` on: an empty list, one element, two elements already in order, two elements reversed, an already sorted list, a reverse-sorted list. After each, `isSortedByPriority()` is true and the size is unchanged.
6. **Stability.** Insert four requests that are identical except `arrivalSeq` and `id`, in scrambled order; after sorting they come out in ascending `arrivalSeq`.
7. **Randomised oracle.** Using `std::mt19937` with a fixed seed of 42, generate 200 requests with unique ids and unique `arrivalSeq` and random urgency (1..4), families (1..50), quantity (1..100). Insert into a `RequestList` and sort. Copy the same requests into a `std::vector`, sort it with `std::sort` and `higherPriority`, and check the ID order matches exactly. Repeat for 5 different seeds. (Using `std::sort` here is allowed because it is test-only.)
8. After `sortByPriority`, `insert` appends at the true end (tail correct); `isSortedByPriority()` becomes false if the new request outranks the old tail and true if it does not.
9. `isSortedByPriority` is false for a list built in a known wrong order.
10. `rankKeyName` returns exactly `"urgency"`, `"families"`, `"quantity"`, `"arrival"`.

### 8.5 DeliveryLog
1. New log is empty, size 0.
2. Append three records; `size()` is 3; `toVector()` is in append order.
3. `searchByCamp` returns only that camp's records, oldest to newest; empty result for an unknown camp.
4. The `path` vector survives intact in a stored record.
5. `clear` empties it and it can be reused.

---

## 9. Build and run (Windows)

The developer machine runs **Windows**. Assume `g++` from MinGW-w64 (installed through MSYS2) and a normal Command Prompt, PowerShell or VS Code terminal. Do **not** depend on `make`, bash scripts, or `-fsanitize` flags. They are not reliably available on this setup.

`build_and_test.bat` must:

1. Compile everything with one explicit command (list the files by name, no wildcards):

```
g++ -std=c++17 -Wall -Wextra -g -I engine -I tests ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp ^
  tests\test_types.cpp tests\test_queue.cpp tests\test_request_list.cpp tests\test_sorting.cpp ^
  tests\test_delivery_log.cpp tests\run_tests.cpp -o run_tests.exe
```

2. Stop with a clear message and a non-zero exit code if compilation fails.
3. Run `run_tests.exe` and pass its exit code through.

The code must compile with **zero warnings** under `-Wall -Wextra`.

Memory safety is checked by the allocation counter from section 7, not by sanitizers. If `-fsanitize=address` happens to work on this machine you may try it as an extra, but it must not be required.

`README.md` states how to run `build_and_test.bat` and gives the single `g++` command.

---

## 10. Definition of done and what to report back

**Done when all of these are true:**

- [ ] Every file in section 3 exists with the names and signatures in this spec.
- [ ] Zero compiler warnings.
- [ ] All tests pass, and the run reports `0 failures`.
- [ ] The allocation counter reports no leaks in any suite.
- [ ] Nothing under `engine/` uses the banned containers or algorithms from section 2.
- [ ] Every public function has its short comment with complexity.
- [ ] Changes are committed with the message `Phase 1: queue, request list, sorting, delivery log` and pushed.

**Report back with:**

1. The exact test output (the final line with the check and failure counts).
2. The list of files created.
3. Anything that was ambiguous or that you changed from this spec, and why. If nothing, say "no deviations".
4. The result of this search, which should return nothing: `Select-String -Path engine\* -Pattern "std::(queue|list|forward_list|priority_queue|deque|map|set|sort)"` (PowerShell)
