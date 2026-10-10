#ifndef AVLTREE_H
#define AVLTREE_H

#include "Types.h"
#include <string>
#include <vector>

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
    // Initializes an empty AVL tree. O(1)
    AVLTree();

    // Frees all nodes in the tree. O(n)
    ~AVLTree();

    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    // Inserts a valid camp; false if !isValidCamp(c) or duplicate id. O(log n)
    bool insert(const Camp& c);

    // Copies the camp with the given id into out; false if not found. O(log n)
    bool search(int id, Camp& out) const;

    // Updates name, x, y, and families for the given camp id; false if not found or invalid. O(log n)
    bool update(int id, const std::string& name, double x, double y, int families);

    // Adds amount to received total for resource r; false if not found or amount <= 0. O(log n)
    bool addReceived(int id, ResourceType r, int amount);

    // Deletes camp by id and rebalances tree; false if not found. O(log n)
    bool remove(int id);

    // Returns total number of camps stored in the tree. O(1)
    int  count() const;

    // Returns true if the tree has no camps. O(1)
    bool isEmpty() const;

    // Returns height of the root, or 0 if the tree is empty. O(1)
    int  treeHeight() const;

    // Returns all camps ordered by ascending id. O(n)
    std::vector<Camp> inorder() const;

    // Populates out with all nodes in preorder (root, left, right). O(n)
    void snapshot(std::vector<AVLNodeInfo>& out) const;

    // Returns rotations performed during the most recent insert or remove. O(1)
    const std::vector<RotationEvent>& lastRotations() const;

    // Validates strict BST ordering, correct heights, and balance factors. O(n)
    bool isValidAVL() const;

    // Deallocates all nodes and resets the tree to empty. O(n)
    void clear();

private:
    struct AVLNode {
        Camp     data;
        int      height;
        AVLNode* left;
        AVLNode* right;
    };

    AVLNode* root;
    int nodeCount;
    std::vector<RotationEvent> rotations;

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

    static void      inorderHelper(const AVLNode* n, std::vector<Camp>& out);
    static void      snapshotHelper(const AVLNode* n, int depth, char side, std::vector<AVLNodeInfo>& out);
    static bool      validateAVL(const AVLNode* n, const AVLNode* minNode, const AVLNode* maxNode, int& computedHeight);
};

#endif // AVLTREE_H
