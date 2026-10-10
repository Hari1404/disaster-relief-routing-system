# Phase 2A Spec: Camp records and the AVL tree

**Project:** ReliefNet, a C++ disaster-relief simulation (college data-structures micro-project)
**This phase:** the `Camp` record and the hand-written AVL tree, with insert, search, update, delete and rotation reporting. No graph, no Dijkstra, no Engine, no console menu, no server, no UI. Those are Phase 2B.
**Depends on:** Phase 1 (committed, all tests passing).
**Time budget:** about 20 minutes

> **Paste this into Antigravity:**
> "Read `docs/PHASE_2A_SPEC.md` and implement it exactly. Follow the file names, function names and signatures as written. Do not add features, rename anything or touch anything outside this spec. The only Phase 1 files you may change are the ones named in section 3, and only in the way described there. When finished, run the tests and report the results as described in section 10. If anything in the spec is unclear or conflicts, stop and ask instead of guessing."

---

## 1. Why this matters (context)

Same as Phase 1. This code will be explained line by line in an oral exam by a student who did not type it. So:

- Keep the code **simple and readable**. Prefer clarity over cleverness.
- Every public function gets a **one or two line comment** saying what it does and its time complexity.
- Every pointer manipulation step gets a short comment.
- `rotateLeft` and `rotateRight` each get a small **ASCII before/after diagram** in a comment above the function, so the student can redraw it on paper.
- The AVL tree must be a **real, hand-written linked tree**. It must not wrap a standard container.

---

## 2. Hard rules

All the Phase 1 hard rules still apply (C++17, no banned containers or `std::sort` in `engine/`, no globals in `engine/`, no `using namespace std;` in headers, destructors free everything, copying disabled, no external libraries, no files that are not listed). In addition:

1. `std::map` and `std::set` are allowed **only in tests**, as oracles to check the tree.
2. `std::string` is allowed in `Camp` and as an output buffer. `std::vector` is allowed as an output buffer.
3. The tree must use **recursion** for insert, remove and the destructor helper. The depth is only about log n, so this is safe.
4. Rotations and rebalancing must be **separate small functions** (`rotateLeft`, `rotateRight`, `rebalance`), not one big function.

---

## 3. Files to create or change

```
engine/
  Types.h          (CHANGE: append only, section 4)
  Types.cpp        (CHANGE: append only, section 4)
  AVLTree.h        (NEW)
  AVLTree.cpp      (NEW)
tests/
  test_avl.cpp     (NEW)
  run_tests.cpp    (CHANGE: add one suite, section 8)
  test_helpers.h   (CHANGE only if the suite declarations live here, see section 8)
build_and_test.bat (CHANGE: add the two new .cpp files to the g++ command)
README.md          (CHANGE: one line saying the AVL tree exists)
```

Do **not** change any existing function in `Types.h`, `Types.cpp` or any other Phase 1 file. Append only.

---

## 4. The `Camp` record (append to `engine/Types.h` and `engine/Types.cpp`)

```cpp
// A relief camp. The AVL tree is keyed by id.
struct Camp {
    int         id;                 // unique, >= 1 (0 is reserved for the depot)
    std::string name;               // not empty
    double      x;                  // position on the map, in km
    double      y;                  // position on the map, in km
    int         families;           // families living in the camp, >= 0
    int         receivedFood;       // total units delivered so far, >= 0
    int         receivedWater;
    int         receivedMedicine;
};

// True only if: id >= 1, name not empty, families >= 0, all three received totals >= 0.
bool isValidCamp(const Camp& c);
```

Add `#include <string>` to `Types.h` if it is not already there.

---

## 5. Conventions (fix these exactly, the tests depend on them)

- **Height of an empty subtree is 0. Height of a leaf is 1.** A node's height is `1 + max(height(left), height(right))`.
- **Balance factor = height(left) minus height(right).** A node is balanced when its balance factor is -1, 0 or +1.
- Each node stores its own height. Update it on the way back up from every insert and remove.
- **Rotation names** are named after where the extra weight is:

| Case | Meaning | Fix |
|---|---|---|
| **LL** | node's balance factor is +2 and its left child's balance factor is >= 0 | `rotateRight(node)` |
| **LR** | node's balance factor is +2 and its left child's balance factor is < 0 | `rotateLeft(node->left)`, then `rotateRight(node)` |
| **RR** | node's balance factor is -2 and its right child's balance factor is <= 0 | `rotateLeft(node)` |
| **RL** | node's balance factor is -2 and its right child's balance factor is > 0 | `rotateRight(node->right)`, then `rotateLeft(node)` |

The "child balance factor equals 0" case can only happen during a **remove**, and it uses the single rotation (LL or RR). The same `rebalance` function is used by insert and remove.

---

## 6. The tree (`engine/AVLTree.h`, `engine/AVLTree.cpp`)

```cpp
enum class RotationType { LL, RR, LR, RL };

// One rebalancing step. nodeId is the id stored in the unbalanced node
// (the node whose balance factor was +2 or -2) when it was detected.
struct RotationEvent {
    RotationType type;
    int          nodeId;
};

// Returns exactly "LL", "RR", "LR" or "RL".
const char* rotationName(RotationType t);

// One node of the tree as the interface will draw it. Produced by snapshot().
struct AVLNodeInfo {
    int  id;
    int  height;
    int  balance;   // balance factor
    int  depth;     // root = 0
    char side;      // 'T' for the root, 'L' for a left child, 'R' for a right child
    int  leftId;    // id of the left child, or 0 if none
    int  rightId;   // id of the right child, or 0 if none
};

class AVLTree {
public:
    AVLTree();
    ~AVLTree();
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    bool insert(const Camp& c);          // false (no change) if !isValidCamp(c) or the id already exists. O(log n)
    bool search(int id, Camp& out) const;   // copy the camp into out; false if not found. O(log n)
    bool update(int id, const std::string& name, double x, double y, int families);
                                         // changes name, x, y, families only. The id and the received totals never change.
                                         // false if not found, name is empty, or families < 0 (nothing changes). O(log n)
    bool addReceived(int id, ResourceType r, int amount);
                                         // adds amount to that camp's received total for resource r.
                                         // false if not found or amount <= 0. O(log n)
    bool remove(int id);                 // delete by id, rebalance. false if not found. O(log n)

    int  count() const;                  // number of camps. O(1)
    bool isEmpty() const;
    int  treeHeight() const;             // height of the root, 0 if empty. O(1)
    std::vector<Camp> inorder() const;   // all camps in ascending id order. O(n)
    void snapshot(std::vector<AVLNodeInfo>& out) const;
                                         // clears out, then appends every node in PREORDER (root first,
                                         // then the whole left subtree, then the whole right subtree). O(n)
    const std::vector<RotationEvent>& lastRotations() const;
                                         // the rotations done by the most recent insert or remove,
                                         // in the order they happened. Empty if there were none.
                                         // Cleared at the START of every insert and remove, including ones that return false.
    bool isValidAVL() const;             // true if: ids are in strict BST order, every stored height is correct,
                                         // and every balance factor is -1, 0 or +1. O(n)
    void clear();                        // free all nodes
};
```

Private parts (names are fixed so the viva cheat sheet can refer to them): a private `AVLNode { Camp data; int height; AVLNode* left; AVLNode* right; }` and these private helpers:

```cpp
static int       height(AVLNode* n);                 // 0 for null
static int       balanceFactor(AVLNode* n);          // height(left) - height(right), 0 for null
static void      updateHeight(AVLNode* n);
static AVLNode*  rotateRight(AVLNode* y);            // returns the new subtree root
static AVLNode*  rotateLeft(AVLNode* x);             // returns the new subtree root
AVLNode*         rebalance(AVLNode* n);              // updates height, fixes imbalance, records the RotationEvent, returns the new subtree root
AVLNode*         insertNode(AVLNode* n, const Camp& c, bool& inserted);
AVLNode*         removeNode(AVLNode* n, int id, bool& removed);
static AVLNode*  findMin(AVLNode* n);                // leftmost node, used by remove
static void      destroy(AVLNode* n);                // post-order delete
```

### Required behaviour

- `insert`: ordinary BST insert by `id`. After the recursive call returns, `rebalance` the node on the way up. A duplicate id returns false.
- `rebalance(n)`: call `updateHeight(n)`, compute the balance factor, pick one of the four cases in section 5, **record a `RotationEvent` before rotating** (one event per case, so a double rotation is **one** event, LR or RL, not two), do the rotation(s), and return the new subtree root. If the node is balanced, return it unchanged and record nothing.
- `rotateRight` and `rotateLeft` update the heights of the two nodes they move (lower node first). They do not record events.
- `remove`: ordinary BST delete with three cases. Leaf: delete it. One child: replace it by the child. Two children: find the in-order successor (`findMin(n->right)`), copy the successor's `Camp` into `n`, then remove the successor's id from the right subtree. After every recursive step, `rebalance` on the way up.
- `remove` of the only node leaves an empty tree (`count() == 0`, `treeHeight() == 0`).
- `snapshot` fills `leftId` / `rightId` with 0 where a child is missing.

---

## 7. Build

Add `engine\AVLTree.cpp` and `tests\test_avl.cpp` to the explicit `g++` command in `build_and_test.bat`. The command becomes:

```
g++ -std=c++17 -Wall -Wextra -g -I engine -I tests ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp engine\AVLTree.cpp ^
  tests\test_types.cpp tests\test_queue.cpp tests\test_request_list.cpp tests\test_sorting.cpp ^
  tests\test_delivery_log.cpp tests\test_avl.cpp tests\run_tests.cpp -o run_tests.exe
```

Zero warnings under `-Wall -Wextra`. Update the single `g++` command in `README.md` to match.

---

## 8. Tests (`tests/test_avl.cpp`, function `runAVLTests()`)

In `run_tests.cpp`, add `runSuite("AVL", runAVLTests);` after the DeliveryLog suite. Declare `runAVLTests` the same way the existing suite functions are declared. The leak counter then checks this suite automatically.

Add a small helper in `test_avl.cpp` (not in `engine/`): `makeCamp(id)` that builds a valid camp with name `"C<id>"`, `x = id`, `y = 0`, `families = 10` and all received totals 0. Another helper: `idsInPreorder(tree)` that returns the ids from `snapshot` as a `std::vector<int>`.

### 8.1 Camp validity
- A valid camp passes `isValidCamp`.
- Each failing alone: `id = 0`, `id = -5`, empty `name`, `families = -1`, `receivedFood = -1`, `receivedWater = -1`, `receivedMedicine = -1`.
- `families = 0` is valid.

### 8.2 Basics
1. A new tree: `isEmpty()`, `count() == 0`, `treeHeight() == 0`, `search(1, c)` false, `remove(1)` false, `inorder()` empty, `snapshot` gives an empty vector, `isValidAVL()` true.
2. Insert one camp: `count() == 1`, `treeHeight() == 1`, `search` finds it and all fields match.
3. Insert a duplicate id: returns false, count unchanged, the stored camp is **not** overwritten (change the name in the second attempt and check the original name remains).
4. Insert an invalid camp (`id = 0`, empty name) returns false and changes nothing.
5. `search` misses an id that was never inserted and an id that was inserted then removed.
6. `inorder()` returns ascending ids after inserting 50, 20, 80, 10, 30, 70, 90 in that order.
7. `clear()` empties the tree (`count() == 0`, `treeHeight() == 0`, `isEmpty()`), and the tree can be filled again afterwards and still passes `isValidAVL()`.

### 8.3 `update` and `addReceived`
1. `update` changes name, x, y and families, and **keeps** the id and all three received totals. (Give the camp non-zero received totals first through `addReceived`.)
2. `update` returns false for an unknown id, an empty name, and `families = -1`, and changes nothing in each case.
3. `addReceived` adds to the right field for Food, Water and Medicine, and adds up over repeated calls (for example 10 then 5 gives 15).
4. `addReceived` returns false for an unknown id, `amount = 0` and `amount = -3`, and changes nothing.

### 8.4 The four rotation cases (hand-checked)
For each case, build a fresh tree, insert the ids in the order shown, then check `lastRotations()` right after the **last** insert, and check the final preorder. Every case ends with the same shape: root 20, left child 10, right child 30, so the preorder is **20, 10, 30**, the tree height is 2, and every node's balance factor is 0.

| Case | Insert order | `lastRotations()` after the last insert |
|---|---|---|
| LL | 30, 20, 10 | exactly one event: LL at node 30 |
| RR | 10, 20, 30 | exactly one event: RR at node 10 |
| LR | 30, 10, 20 | exactly one event: LR at node 30 |
| RL | 10, 30, 20 | exactly one event: RL at node 10 |

Also check that inserting the first two ids of each sequence leaves `lastRotations()` empty (no rotation yet) and that `rotationName` returns exactly `"LL"`, `"RR"`, `"LR"`, `"RL"`.

### 8.5 The scenario 4 sequence (this is the viva demo)
Insert 10, 20, 30, 40, 50, 25 in that order. Check `lastRotations()` after **every** insert:

| After inserting | `lastRotations()` |
|---|---|
| 10 | empty |
| 20 | empty |
| 30 | RR at node 10 |
| 40 | empty |
| 50 | RR at node 30 |
| 25 | RL at node 20 |

Final preorder must be exactly **30, 20, 10, 25, 40, 50**. Check the `snapshot` details for every node as (id, height, balance):

(30, 3, 0), (20, 2, 0), (10, 1, 0), (25, 1, 0), (40, 2, -1), (50, 1, 0)

Also check `side`, `depth`, `leftId` and `rightId` for the root and node 40: root is `'T'`, depth 0, `leftId` 20, `rightId` 40. Node 40 is `'R'`, depth 1, `leftId` 0, `rightId` 50.

### 8.6 Remove, one case per test (hand-checked)
Use this **base tree** where it says so: insert 50, 30, 70, 20, 40, 60, 80. It is perfectly balanced (no rotations), preorder **50, 30, 20, 40, 70, 60, 80**, height 3.

| Test | Starting tree | Remove | Preorder after | Rotations |
|---|---|---|---|---|
| leaf | base | 20 | 50, 30, 40, 70, 60, 80 | none |
| one child | base, then insert 10 | 20 | 50, 30, 10, 40, 70, 60, 80 | none |
| two children | base | 30 | 50, 40, 20, 70, 60, 80 | none |
| root with two children | base | 50 | 60, 30, 20, 40, 70, 80 | none |
| root whose successor has a right child | base, then insert 65 | 50 | 60, 30, 20, 40, 70, 65, 80 | none |

In the "two children" row, the node 30 is replaced by its in-order successor 40. In the "root" rows, the successor is 60. After each remove check `count()`, that the removed id is gone from `search`, that `inorder()` is still ascending, and that `isValidAVL()` is true.

Also: removing an id that is not in the tree returns false and changes nothing. Removing the only node leaves the tree empty and valid. After a remove with no rotation, `lastRotations()` is empty.

### 8.7 Remove that needs a rotation (hand-checked)
Build, remove, then check `lastRotations()` and the preorder.

| Test | Insert order | Remove | Rotation | Preorder after |
|---|---|---|---|---|
| LL | 20, 10, 30, 5 | 30 | LL at node 20 | 10, 5, 20 |
| LR | 20, 10, 30, 15 | 30 | LR at node 20 | 15, 10, 20 |
| RR | 20, 10, 30, 35 | 10 | RR at node 20 | 30, 20, 35 |
| RL | 20, 10, 30, 25 | 10 | RL at node 20 | 25, 20, 30 |
| LL, child balance 0 | 20, 10, 30, 5, 15 | 30 | LL at node 20 | 10, 5, 20, 15 |
| RR, child balance 0 | 20, 10, 30, 25, 35 | 10 | RR at node 20 | 30, 20, 25, 35 |

For the two "child balance 0" rows also check the `snapshot` (id, height, balance): the LL one gives (10, 3, -1), (5, 1, 0), (20, 2, 1), (15, 1, 0). The RR one gives (30, 3, 1), (20, 2, -1), (25, 1, 0), (35, 1, 0). These rows prove that a child balance factor of 0 uses a single rotation. After each, `isValidAVL()` is true.

### 8.8 Why AVL and not a plain BST
1. Insert 1, 2, 3, ..., 1023 in ascending order. `count() == 1023`, `isValidAVL()` is true, and `treeHeight() == 10`. (A plain BST would have height 1023. Add that as a comment in the test.)
2. Insert 1..100 ascending: `isValidAVL()` true and `treeHeight() == 7`.
3. Insert 1..1000 ascending then remove all the even ids: `count() == 500`, `isValidAVL()` true, and `inorder()` is exactly the odd ids ascending.

### 8.9 Randomised oracle
Using `std::mt19937` with a fixed seed of 42: do 2000 operations. Each operation picks a random id in 1..150 and a random choice of insert or remove. Keep a `std::set<int>` oracle (allowed in tests). After **every** operation check:
- the return value matches the oracle (insert returns true only if the id was not in the set; remove returns true only if it was),
- `count()` equals the oracle size,
- `isValidAVL()` is true,
- `inorder()` ids equal the oracle's ids in order. (This one may be checked every 50th operation to keep the test fast.)

Repeat for the seeds 1, 2, 3, 4 and 42 (five runs).

### 8.10 Mutation check (not a test, a report item)
Do this **only after** the work is committed and pushed (section 10), so nothing is lost. Temporarily comment out the `updateHeight(n);` call inside `rebalance`, rebuild, and confirm that the AVL suite now **fails**. Then restore the file with `git restore engine`, rebuild, and confirm all tests pass again. Report both results. This proves the tests actually test the balancing.

---

## 9. Style checklist

- Every public function has its short comment with complexity.
- Every rotation function has the ASCII before/after diagram.
- Every `rebalance` case has a one-line comment saying which case it is.
- No function longer than about 40 lines.
- No raw `new` or `delete` outside `AVLTree.cpp`.

---

## 10. Definition of done and what to report back

**Done when all of these are true:**

- [ ] Every file in section 3 exists or is changed as described, with the names and signatures in this spec.
- [ ] Zero compiler warnings.
- [ ] All tests pass (Phase 1 suites and the new AVL suite), and the run reports `0 failures`.
- [ ] The allocation counter reports no leaks in any suite, including AVL.
- [ ] Nothing under `engine/` uses the banned containers or algorithms.
- [ ] No existing Phase 1 function was modified.
- [ ] Changes are committed with the message `Phase 2A: camp record and AVL tree` and pushed.
- [ ] After that commit, the mutation check in 8.10 was done and the files were restored (the working tree is clean again).

**Report back with:**

1. The exact test output (the final line with the check and failure counts).
2. The list of files created and the list of files changed.
3. The result of the mutation check (8.10): what failed when `updateHeight` was removed.
4. Anything that was ambiguous or that you changed from this spec, and why. If nothing, say "no deviations".
5. The result of this search, which should return nothing: `Select-String -Path engine\* -Pattern "std::(queue|list|forward_list|priority_queue|deque|map|set|sort|unordered_map)"` (PowerShell)
