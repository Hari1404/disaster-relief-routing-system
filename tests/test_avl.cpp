#include "test_helpers.h"
#include "AVLTree.h"
#include <vector>
#include <string>
#include <random>
#include <set>
#include <limits>

// Builds a valid camp with name "C<id>", x = id, y = 0, families = 10, received totals 0.
static Camp makeCamp(int id) {
    Camp c;
    c.id = id;
    c.name = "C" + std::to_string(id);
    c.x = static_cast<double>(id);
    c.y = 0.0;
    c.families = 10;
    c.receivedFood = 0;
    c.receivedWater = 0;
    c.receivedMedicine = 0;
    return c;
}

// Returns the ids from snapshot as a std::vector<int>.
static std::vector<int> idsInPreorder(const AVLTree& tree) {
    std::vector<AVLNodeInfo> snap;
    tree.snapshot(snap);
    std::vector<int> ids;
    ids.reserve(snap.size());
    for (const auto& node : snap) {
        ids.push_back(node.id);
    }
    return ids;
}

// 8.1 Camp validity
static void testCampValidity() {
    Camp c = makeCamp(1);
    CHECK(isValidCamp(c));

    // Each failing alone
    Camp cBad = c;
    cBad.id = 0;
    CHECK(!isValidCamp(cBad));

    cBad = c;
    cBad.id = -5;
    CHECK(!isValidCamp(cBad));

    cBad = c;
    cBad.name = "";
    CHECK(!isValidCamp(cBad));

    cBad = c;
    cBad.families = -1;
    CHECK(!isValidCamp(cBad));

    cBad = c;
    cBad.receivedFood = -1;
    CHECK(!isValidCamp(cBad));

    cBad = c;
    cBad.receivedWater = -1;
    CHECK(!isValidCamp(cBad));

    cBad = c;
    cBad.receivedMedicine = -1;
    CHECK(!isValidCamp(cBad));

    // families = 0 is valid
    cBad = c;
    cBad.families = 0;
    CHECK(isValidCamp(cBad));
}

// 8.2 Basics
static void testBasics() {
    // 1. A new tree
    AVLTree tree;
    Camp dummy;
    CHECK(tree.isEmpty());
    CHECK(tree.count() == 0);
    CHECK(tree.treeHeight() == 0);
    CHECK(!tree.search(1, dummy));
    CHECK(!tree.remove(1));
    CHECK(tree.inorder().empty());
    std::vector<AVLNodeInfo> snap;
    tree.snapshot(snap);
    CHECK(snap.empty());
    CHECK(tree.isValidAVL());

    // 2. Insert one camp
    Camp c1 = makeCamp(1);
    CHECK(tree.insert(c1));
    CHECK(tree.count() == 1);
    CHECK(tree.treeHeight() == 1);
    Camp found;
    CHECK(tree.search(1, found));
    CHECK(found.id == c1.id);
    CHECK(found.name == c1.name);
    CHECK(found.x == c1.x);
    CHECK(found.y == c1.y);
    CHECK(found.families == c1.families);
    CHECK(found.receivedFood == c1.receivedFood);
    CHECK(found.receivedWater == c1.receivedWater);
    CHECK(found.receivedMedicine == c1.receivedMedicine);

    // 3. Insert a duplicate id
    Camp c1Dup = makeCamp(1);
    c1Dup.name = "DifferentName";
    CHECK(!tree.insert(c1Dup));
    CHECK(tree.count() == 1);
    CHECK(tree.search(1, found));
    CHECK(found.name == "C1");

    // 4. Insert an invalid camp
    Camp inv1 = makeCamp(0);
    CHECK(!tree.insert(inv1));
    Camp inv2 = makeCamp(2);
    inv2.name = "";
    CHECK(!tree.insert(inv2));
    CHECK(tree.count() == 1);

    // 5. search misses
    CHECK(!tree.search(999, dummy));
    tree.insert(makeCamp(999));
    CHECK(tree.search(999, dummy));
    CHECK(tree.remove(999));
    CHECK(!tree.search(999, dummy));

    // 6. inorder() returns ascending ids
    AVLTree t6;
    int idsToInsert[] = {50, 20, 80, 10, 30, 70, 90};
    for (int id : idsToInsert) {
        CHECK(t6.insert(makeCamp(id)));
    }
    std::vector<Camp> in = t6.inorder();
    CHECK(in.size() == 7);
    int expectedInorder[] = {10, 20, 30, 50, 70, 80, 90};
    for (size_t i = 0; i < 7; ++i) {
        CHECK(in[i].id == expectedInorder[i]);
    }

    // 7. clear() empties the tree
    t6.clear();
    CHECK(t6.count() == 0);
    CHECK(t6.treeHeight() == 0);
    CHECK(t6.isEmpty());
    CHECK(t6.insert(makeCamp(42)));
    CHECK(t6.insert(makeCamp(24)));
    CHECK(t6.count() == 2);
    CHECK(t6.isValidAVL());
}

// 8.3 update and addReceived
static void testUpdateAndAddReceived() {
    AVLTree tree;
    tree.insert(makeCamp(10));

    // 1. Give camp non-zero received totals first
    CHECK(tree.addReceived(10, ResourceType::Food, 5));
    CHECK(tree.addReceived(10, ResourceType::Water, 12));
    CHECK(tree.addReceived(10, ResourceType::Medicine, 7));

    CHECK(tree.update(10, "NewCamp", 15.5, 25.5, 99));
    Camp c;
    CHECK(tree.search(10, c));
    CHECK(c.id == 10);
    CHECK(c.name == "NewCamp");
    CHECK(c.x == 15.5);
    CHECK(c.y == 25.5);
    CHECK(c.families == 99);
    CHECK(c.receivedFood == 5);
    CHECK(c.receivedWater == 12);
    CHECK(c.receivedMedicine == 7);

    // 2. update returns false and changes nothing
    CHECK(!tree.update(999, "Unknown", 0.0, 0.0, 10));
    CHECK(!tree.update(10, "", 1.0, 1.0, 10));
    CHECK(!tree.update(10, "ValidName", 1.0, 1.0, -1));
    CHECK(!tree.update(10, "ValidName", std::numeric_limits<double>::quiet_NaN(), 1.0, 10));
    CHECK(!tree.update(10, "ValidName", 1.0, std::numeric_limits<double>::infinity(), 10));

    CHECK(tree.search(10, c));
    CHECK(c.name == "NewCamp");
    CHECK(c.families == 99);
    CHECK(c.x == 15.5 && c.y == 25.5);   // the rejected updates left the position alone

    // 3. addReceived adds up
    CHECK(tree.addReceived(10, ResourceType::Food, 10));
    CHECK(tree.addReceived(10, ResourceType::Food, 5));
    CHECK(tree.search(10, c));
    CHECK(c.receivedFood == 20);

    // 4. addReceived returns false and changes nothing
    CHECK(!tree.addReceived(999, ResourceType::Food, 10));
    CHECK(!tree.addReceived(10, ResourceType::Food, 0));
    CHECK(!tree.addReceived(10, ResourceType::Food, -3));
    CHECK(tree.search(10, c));
    CHECK(c.receivedFood == 20);
}

// 8.4 The four rotation cases
static void testRotations() {
    CHECK(std::string(rotationName(RotationType::LL)) == "LL");
    CHECK(std::string(rotationName(RotationType::RR)) == "RR");
    CHECK(std::string(rotationName(RotationType::LR)) == "LR");
    CHECK(std::string(rotationName(RotationType::RL)) == "RL");

    std::vector<int> expectedPreorder = {20, 10, 30};

    // LL: 30, 20, 10
    {
        AVLTree tree;
        tree.insert(makeCamp(30));
        CHECK(tree.lastRotations().empty());
        tree.insert(makeCamp(20));
        CHECK(tree.lastRotations().empty());
        tree.insert(makeCamp(10));
        const auto& rots = tree.lastRotations();
        CHECK(rots.size() == 1);
        CHECK(rots[0].type == RotationType::LL);
        CHECK(rots[0].nodeId == 30);
        CHECK(idsInPreorder(tree) == expectedPreorder);
        CHECK(tree.treeHeight() == 2);
        CHECK(tree.isValidAVL());
    }

    // RR: 10, 20, 30
    {
        AVLTree tree;
        tree.insert(makeCamp(10));
        CHECK(tree.lastRotations().empty());
        tree.insert(makeCamp(20));
        CHECK(tree.lastRotations().empty());
        tree.insert(makeCamp(30));
        const auto& rots = tree.lastRotations();
        CHECK(rots.size() == 1);
        CHECK(rots[0].type == RotationType::RR);
        CHECK(rots[0].nodeId == 10);
        CHECK(idsInPreorder(tree) == expectedPreorder);
        CHECK(tree.treeHeight() == 2);
        CHECK(tree.isValidAVL());
    }

    // LR: 30, 10, 20
    {
        AVLTree tree;
        tree.insert(makeCamp(30));
        CHECK(tree.lastRotations().empty());
        tree.insert(makeCamp(10));
        CHECK(tree.lastRotations().empty());
        tree.insert(makeCamp(20));
        const auto& rots = tree.lastRotations();
        CHECK(rots.size() == 1);
        CHECK(rots[0].type == RotationType::LR);
        CHECK(rots[0].nodeId == 30);
        CHECK(idsInPreorder(tree) == expectedPreorder);
        CHECK(tree.treeHeight() == 2);
        CHECK(tree.isValidAVL());
    }

    // RL: 10, 30, 20
    {
        AVLTree tree;
        tree.insert(makeCamp(10));
        CHECK(tree.lastRotations().empty());
        tree.insert(makeCamp(30));
        CHECK(tree.lastRotations().empty());
        tree.insert(makeCamp(20));
        const auto& rots = tree.lastRotations();
        CHECK(rots.size() == 1);
        CHECK(rots[0].type == RotationType::RL);
        CHECK(rots[0].nodeId == 10);
        CHECK(idsInPreorder(tree) == expectedPreorder);
        CHECK(tree.treeHeight() == 2);
        CHECK(tree.isValidAVL());
    }
}

// 8.5 The scenario 4 sequence
static void testScenario4() {
    AVLTree tree;

    // After 10: empty
    tree.insert(makeCamp(10));
    CHECK(tree.lastRotations().empty());

    // After 20: empty
    tree.insert(makeCamp(20));
    CHECK(tree.lastRotations().empty());

    // After 30: RR at node 10
    tree.insert(makeCamp(30));
    CHECK(tree.lastRotations().size() == 1);
    CHECK(tree.lastRotations()[0].type == RotationType::RR);
    CHECK(tree.lastRotations()[0].nodeId == 10);

    // After 40: empty
    tree.insert(makeCamp(40));
    CHECK(tree.lastRotations().empty());

    // After 50: RR at node 30
    tree.insert(makeCamp(50));
    CHECK(tree.lastRotations().size() == 1);
    CHECK(tree.lastRotations()[0].type == RotationType::RR);
    CHECK(tree.lastRotations()[0].nodeId == 30);

    // After 25: RL at node 20
    tree.insert(makeCamp(25));
    CHECK(tree.lastRotations().size() == 1);
    CHECK(tree.lastRotations()[0].type == RotationType::RL);
    CHECK(tree.lastRotations()[0].nodeId == 20);

    // Final preorder must be exactly 30, 20, 10, 25, 40, 50
    std::vector<int> expectedPreorder = {30, 20, 10, 25, 40, 50};
    CHECK(idsInPreorder(tree) == expectedPreorder);

    // Check snapshot details for every node as (id, height, balance)
    std::vector<AVLNodeInfo> snap;
    tree.snapshot(snap);
    CHECK(snap.size() == 6);

    struct ExpectedNode { int id; int height; int balance; };
    ExpectedNode expectedSnap[6] = {
        {30, 3, 0},
        {20, 2, 0},
        {10, 1, 0},
        {25, 1, 0},
        {40, 2, -1},
        {50, 1, 0}
    };
    for (size_t i = 0; i < 6; ++i) {
        CHECK(snap[i].id == expectedSnap[i].id);
        CHECK(snap[i].height == expectedSnap[i].height);
        CHECK(snap[i].balance == expectedSnap[i].balance);
    }

    // Root is 'T', depth 0, leftId 20, rightId 40
    CHECK(snap[0].id == 30);
    CHECK(snap[0].side == 'T');
    CHECK(snap[0].depth == 0);
    CHECK(snap[0].leftId == 20);
    CHECK(snap[0].rightId == 40);

    // Node 40 is 'R', depth 1, leftId 0, rightId 50
    CHECK(snap[4].id == 40);
    CHECK(snap[4].side == 'R');
    CHECK(snap[4].depth == 1);
    CHECK(snap[4].leftId == 0);
    CHECK(snap[4].rightId == 50);
}

static void buildBaseTree(AVLTree& tree) {
    int ids[] = {50, 30, 70, 20, 40, 60, 80};
    for (int id : ids) {
        tree.insert(makeCamp(id));
    }
}

static bool isAscending(const std::vector<Camp>& vec) {
    for (size_t i = 1; i < vec.size(); ++i) {
        if (vec[i].id <= vec[i - 1].id) return false;
    }
    return true;
}

// 8.6 Remove, one case per test
static void testRemoveCases() {
    Camp dummy;

    // Base tree check
    {
        AVLTree base;
        buildBaseTree(base);
        std::vector<int> expectedBase = {50, 30, 20, 40, 70, 60, 80};
        CHECK(idsInPreorder(base) == expectedBase);
        CHECK(base.treeHeight() == 3);
        CHECK(base.lastRotations().empty());
    }

    // Leaf: remove 20
    {
        AVLTree tree;
        buildBaseTree(tree);
        CHECK(tree.remove(20));
        CHECK(tree.count() == 6);
        CHECK(!tree.search(20, dummy));
        CHECK(isAscending(tree.inorder()));
        CHECK(tree.isValidAVL());
        CHECK(tree.lastRotations().empty());
        std::vector<int> expected = {50, 30, 40, 70, 60, 80};
        CHECK(idsInPreorder(tree) == expected);
    }

    // One child: base, then insert 10, remove 20
    {
        AVLTree tree;
        buildBaseTree(tree);
        tree.insert(makeCamp(10));
        CHECK(tree.remove(20));
        CHECK(tree.count() == 7);
        CHECK(!tree.search(20, dummy));
        CHECK(isAscending(tree.inorder()));
        CHECK(tree.isValidAVL());
        CHECK(tree.lastRotations().empty());
        std::vector<int> expected = {50, 30, 10, 40, 70, 60, 80};
        CHECK(idsInPreorder(tree) == expected);
    }

    // Two children: base, remove 30
    {
        AVLTree tree;
        buildBaseTree(tree);
        CHECK(tree.remove(30));
        CHECK(tree.count() == 6);
        CHECK(!tree.search(30, dummy));
        CHECK(isAscending(tree.inorder()));
        CHECK(tree.isValidAVL());
        CHECK(tree.lastRotations().empty());
        std::vector<int> expected = {50, 40, 20, 70, 60, 80};
        CHECK(idsInPreorder(tree) == expected);
    }

    // Root with two children: base, remove 50
    {
        AVLTree tree;
        buildBaseTree(tree);
        CHECK(tree.remove(50));
        CHECK(tree.count() == 6);
        CHECK(!tree.search(50, dummy));
        CHECK(isAscending(tree.inorder()));
        CHECK(tree.isValidAVL());
        CHECK(tree.lastRotations().empty());
        std::vector<int> expected = {60, 30, 20, 40, 70, 80};
        CHECK(idsInPreorder(tree) == expected);
    }

    // Root whose successor has a right child: base, insert 65, remove 50
    {
        AVLTree tree;
        buildBaseTree(tree);
        tree.insert(makeCamp(65));
        CHECK(tree.remove(50));
        CHECK(tree.count() == 7);
        CHECK(!tree.search(50, dummy));
        CHECK(isAscending(tree.inorder()));
        CHECK(tree.isValidAVL());
        CHECK(tree.lastRotations().empty());
        std::vector<int> expected = {60, 30, 20, 40, 70, 65, 80};
        CHECK(idsInPreorder(tree) == expected);
    }

    // Removing id not in tree
    {
        AVLTree tree;
        buildBaseTree(tree);
        CHECK(!tree.remove(999));
        CHECK(tree.count() == 7);
        CHECK(tree.isValidAVL());
        CHECK(tree.lastRotations().empty());
    }

    // Removing only node
    {
        AVLTree tree;
        tree.insert(makeCamp(100));
        CHECK(tree.remove(100));
        CHECK(tree.isEmpty());
        CHECK(tree.count() == 0);
        CHECK(tree.treeHeight() == 0);
        CHECK(tree.isValidAVL());
        CHECK(tree.lastRotations().empty());
    }
}

// 8.7 Remove that needs a rotation
static void testRemoveWithRotation() {
    // LL
    {
        AVLTree tree;
        int ins[] = {20, 10, 30, 5};
        for (int id : ins) tree.insert(makeCamp(id));
        CHECK(tree.remove(30));
        CHECK(tree.lastRotations().size() == 1);
        CHECK(tree.lastRotations()[0].type == RotationType::LL);
        CHECK(tree.lastRotations()[0].nodeId == 20);
        std::vector<int> expected = {10, 5, 20};
        CHECK(idsInPreorder(tree) == expected);
        CHECK(tree.isValidAVL());
    }

    // LR
    {
        AVLTree tree;
        int ins[] = {20, 10, 30, 15};
        for (int id : ins) tree.insert(makeCamp(id));
        CHECK(tree.remove(30));
        CHECK(tree.lastRotations().size() == 1);
        CHECK(tree.lastRotations()[0].type == RotationType::LR);
        CHECK(tree.lastRotations()[0].nodeId == 20);
        std::vector<int> expected = {15, 10, 20};
        CHECK(idsInPreorder(tree) == expected);
        CHECK(tree.isValidAVL());
    }

    // RR
    {
        AVLTree tree;
        int ins[] = {20, 10, 30, 35};
        for (int id : ins) tree.insert(makeCamp(id));
        CHECK(tree.remove(10));
        CHECK(tree.lastRotations().size() == 1);
        CHECK(tree.lastRotations()[0].type == RotationType::RR);
        CHECK(tree.lastRotations()[0].nodeId == 20);
        std::vector<int> expected = {30, 20, 35};
        CHECK(idsInPreorder(tree) == expected);
        CHECK(tree.isValidAVL());
    }

    // RL
    {
        AVLTree tree;
        int ins[] = {20, 10, 30, 25};
        for (int id : ins) tree.insert(makeCamp(id));
        CHECK(tree.remove(10));
        CHECK(tree.lastRotations().size() == 1);
        CHECK(tree.lastRotations()[0].type == RotationType::RL);
        CHECK(tree.lastRotations()[0].nodeId == 20);
        std::vector<int> expected = {25, 20, 30};
        CHECK(idsInPreorder(tree) == expected);
        CHECK(tree.isValidAVL());
    }

    // LL, child balance 0
    {
        AVLTree tree;
        int ins[] = {20, 10, 30, 5, 15};
        for (int id : ins) tree.insert(makeCamp(id));
        CHECK(tree.remove(30));
        CHECK(tree.lastRotations().size() == 1);
        CHECK(tree.lastRotations()[0].type == RotationType::LL);
        CHECK(tree.lastRotations()[0].nodeId == 20);
        std::vector<int> expected = {10, 5, 20, 15};
        CHECK(idsInPreorder(tree) == expected);
        CHECK(tree.isValidAVL());

        std::vector<AVLNodeInfo> snap;
        tree.snapshot(snap);
        CHECK(snap.size() == 4);
        CHECK(snap[0].id == 10 && snap[0].height == 3 && snap[0].balance == -1);
        CHECK(snap[1].id == 5 && snap[1].height == 1 && snap[1].balance == 0);
        CHECK(snap[2].id == 20 && snap[2].height == 2 && snap[2].balance == 1);
        CHECK(snap[3].id == 15 && snap[3].height == 1 && snap[3].balance == 0);
    }

    // RR, child balance 0
    {
        AVLTree tree;
        int ins[] = {20, 10, 30, 25, 35};
        for (int id : ins) tree.insert(makeCamp(id));
        CHECK(tree.remove(10));
        CHECK(tree.lastRotations().size() == 1);
        CHECK(tree.lastRotations()[0].type == RotationType::RR);
        CHECK(tree.lastRotations()[0].nodeId == 20);
        std::vector<int> expected = {30, 20, 25, 35};
        CHECK(idsInPreorder(tree) == expected);
        CHECK(tree.isValidAVL());

        std::vector<AVLNodeInfo> snap;
        tree.snapshot(snap);
        CHECK(snap.size() == 4);
        CHECK(snap[0].id == 30 && snap[0].height == 3 && snap[0].balance == 1);
        CHECK(snap[1].id == 20 && snap[1].height == 2 && snap[1].balance == -1);
        CHECK(snap[2].id == 25 && snap[2].height == 1 && snap[2].balance == 0);
        CHECK(snap[3].id == 35 && snap[3].height == 1 && snap[3].balance == 0);
    }
}

// 8.8 Why AVL and not a plain BST
static void testWhyAVLNotBST() {
    // 1. Insert 1, 2, 3, ..., 1023 in ascending order.
    {
        AVLTree tree;
        for (int i = 1; i <= 1023; ++i) {
            tree.insert(makeCamp(i));
        }
        CHECK(tree.count() == 1023);
        CHECK(tree.isValidAVL());
        CHECK(tree.treeHeight() == 10);
        // A plain BST would have height 1023.
    }

    // 2. Insert 1..100 ascending
    {
        AVLTree tree;
        for (int i = 1; i <= 100; ++i) {
            tree.insert(makeCamp(i));
        }
        CHECK(tree.isValidAVL());
        CHECK(tree.treeHeight() == 7);
    }

    // 3. Insert 1..1000 ascending then remove all even ids
    {
        AVLTree tree;
        for (int i = 1; i <= 1000; ++i) {
            tree.insert(makeCamp(i));
        }
        for (int i = 2; i <= 1000; i += 2) {
            tree.remove(i);
        }
        CHECK(tree.count() == 500);
        CHECK(tree.isValidAVL());
        std::vector<Camp> in = tree.inorder();
        CHECK(in.size() == 500);
        bool match = true;
        for (int i = 0; i < 500; ++i) {
            if (in[i].id != 2 * i + 1) {
                match = false;
                break;
            }
        }
        CHECK(match);
    }
}

// 8.9 Randomised oracle
static void testRandomizedOracle() {
    uint32_t seeds[] = {1, 2, 3, 4, 42};
    for (uint32_t seed : seeds) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> idDist(1, 150);
        std::uniform_int_distribution<int> opDist(0, 1);

        AVLTree tree;
        std::set<int> oracle;

        for (int op = 0; op < 2000; ++op) {
            int id = idDist(rng);
            int action = opDist(rng); // 0 = insert, 1 = remove

            if (action == 0) {
                bool expected = (oracle.find(id) == oracle.end());
                bool actual = tree.insert(makeCamp(id));
                CHECK(actual == expected);
                if (expected) {
                    oracle.insert(id);
                }
            } else {
                bool expected = (oracle.find(id) != oracle.end());
                bool actual = tree.remove(id);
                CHECK(actual == expected);
                if (expected) {
                    oracle.erase(id);
                }
            }

            CHECK(tree.count() == static_cast<int>(oracle.size()));
            CHECK(tree.isValidAVL());

            if (op % 50 == 0) {
                std::vector<Camp> in = tree.inorder();
                CHECK(in.size() == oracle.size());
                auto it = oracle.begin();
                bool matches = true;
                for (size_t i = 0; i < in.size(); ++i, ++it) {
                    if (in[i].id != *it) {
                        matches = false;
                        break;
                    }
                }
                CHECK(matches);
            }
        }
    }
}

// Regression tests: stale rotations after clear (BUG-4), received totals that must not overflow (BUG-5),
// and update() refusing coordinates outside the allowed range (BUG-2).
static void testClearAndSaturation() {
    AVLTree tree;
    for (int id : {10, 20, 30}) tree.insert(makeCamp(id));
    CHECK(!tree.lastRotations().empty());     // 10, 20, 30 in order forces one rotation
    tree.clear();
    CHECK(tree.count() == 0);
    CHECK(tree.lastRotations().empty());      // the old rotation must not survive a clear

    const int big = std::numeric_limits<int>::max();
    tree.insert(makeCamp(10));
    CHECK(tree.addReceived(10, ResourceType::Food, big));
    CHECK(tree.addReceived(10, ResourceType::Food, big));       // would overflow without the cap
    CHECK(tree.addReceived(10, ResourceType::Food, 1));
    CHECK(tree.addReceived(10, ResourceType::Water, big));
    CHECK(tree.addReceived(10, ResourceType::Water, 5));
    CHECK(tree.addReceived(10, ResourceType::Medicine, big - 1));
    CHECK(tree.addReceived(10, ResourceType::Medicine, 10));
    Camp c;
    CHECK(tree.search(10, c));
    CHECK(c.receivedFood == big && c.receivedWater == big && c.receivedMedicine == big);

    // Moving a camp out of the allowed range is refused and changes nothing
    Camp before;
    CHECK(tree.search(10, before));
    CHECK(!tree.update(10, "moved", 1e200, 0.0, 5));
    CHECK(!tree.update(10, "moved", 0.0, 1000001.0, 5));
    CHECK(tree.search(10, c) && c.x == before.x && c.y == before.y && c.name == before.name);
    CHECK(tree.update(10, "moved", 1000000.0, -1000000.0, 5));  // exactly on the limit is fine
}

void runAVLTests() {
    testClearAndSaturation();
    testCampValidity();
    testBasics();
    testUpdateAndAddReceived();
    testRotations();
    testScenario4();
    testRemoveCases();
    testRemoveWithRotation();
    testWhyAVLNotBST();
    testRandomizedOracle();
}
