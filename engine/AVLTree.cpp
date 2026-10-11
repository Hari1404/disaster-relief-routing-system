#include "AVLTree.h"
#include <climits>
#include <cmath>

// total + amount, but stops at INT_MAX instead of overflowing (signed overflow is undefined behaviour).
// Both arguments are >= 0 here. O(1)
static int saturatingAdd(int total, int amount) {
    return (total > INT_MAX - amount) ? INT_MAX : total + amount;
}

// Returns exactly "LL", "RR", "LR" or "RL".
const char* rotationName(RotationType t) {
    switch (t) {
        case RotationType::LL: return "LL";
        case RotationType::RR: return "RR";
        case RotationType::LR: return "LR";
        case RotationType::RL: return "RL";
    }
    return "";
}

// Initializes an empty AVL tree. O(1)
AVLTree::AVLTree() : root(nullptr), nodeCount(0), rotations() {}

// Frees all nodes in the tree. O(n)
AVLTree::~AVLTree() {
    clear();
}

// Deallocates all nodes and resets the tree to empty. O(n)
void AVLTree::clear() {
    destroy(root);
    root = nullptr;
    nodeCount = 0;
    rotations.clear();   // no stale rotation report after a reset
}

// Post-order traversal to delete every node in the subtree. O(n)
void AVLTree::destroy(AVLNode* n) {
    if (!n) return;
    // Recursively delete left subtree
    destroy(n->left);
    // Recursively delete right subtree
    destroy(n->right);
    // Delete current node
    delete n;
}

// Returns height of node n, or 0 if n is nullptr. O(1)
int AVLTree::height(AVLNode* n) {
    return n ? n->height : 0;
}

// Returns balance factor: height(left) - height(right), 0 for null. O(1)
int AVLTree::balanceFactor(AVLNode* n) {
    return n ? height(n->left) - height(n->right) : 0;
}

// Updates height of node n based on heights of left and right children. O(1)
void AVLTree::updateHeight(AVLNode* n) {
    if (!n) return;
    int lh = height(n->left);
    int rh = height(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

/*
 * Right rotation at node y:
 *
 *       y                x
 *      / \              / \
 *     x   T3   -->     T1  y
 *    / \                  / \
 *   T1  T2               T2  T3
 */
AVLTree::AVLNode* AVLTree::rotateRight(AVLNode* y) {
    // x becomes the new subtree root
    AVLNode* x = y->left;
    // T2 moves to become y's left child
    y->left = x->right;
    // y becomes x's right child
    x->right = y;

    // Update heights (lower node y first, then new root x)
    updateHeight(y);
    updateHeight(x);

    return x;
}

/*
 * Left rotation at node x:
 *
 *     x                  y
 *    / \                / \
 *   T1  y      -->     x   T3
 *      / \            / \
 *     T2  T3         T1  T2
 */
AVLTree::AVLNode* AVLTree::rotateLeft(AVLNode* x) {
    // y becomes the new subtree root
    AVLNode* y = x->right;
    // T2 moves to become x's right child
    x->right = y->left;
    // x becomes y's left child
    y->left = x;

    // Update heights (lower node x first, then new root y)
    updateHeight(x);
    updateHeight(y);

    return y;
}

// Updates height, fixes imbalance, records RotationEvent, returns new root. O(1)
AVLTree::AVLNode* AVLTree::rebalance(AVLNode* n) {
    if (!n) return nullptr;

    updateHeight(n);
    int bf = balanceFactor(n);

    // Left-heavy: balance factor is +2
    if (bf == 2) {
        if (balanceFactor(n->left) >= 0) {
            // Case LL: left child is left-heavy or balanced (single right rotation)
            rotations.push_back({RotationType::LL, n->data.id});
            return rotateRight(n);
        } else {
            // Case LR: left child is right-heavy (left rotation on child, then right rotation)
            rotations.push_back({RotationType::LR, n->data.id});
            n->left = rotateLeft(n->left);
            return rotateRight(n);
        }
    }

    // Right-heavy: balance factor is -2
    if (bf == -2) {
        if (balanceFactor(n->right) <= 0) {
            // Case RR: right child is right-heavy or balanced (single left rotation)
            rotations.push_back({RotationType::RR, n->data.id});
            return rotateLeft(n);
        } else {
            // Case RL: right child is left-heavy (right rotation on child, then left rotation)
            rotations.push_back({RotationType::RL, n->data.id});
            n->right = rotateRight(n->right);
            return rotateLeft(n);
        }
    }

    // Balanced node (-1, 0, +1): return unchanged
    return n;
}

// Inserts a valid camp; false if !isValidCamp(c) or duplicate id. O(log n)
bool AVLTree::insert(const Camp& c) {
    rotations.clear();
    if (!isValidCamp(c)) {
        return false;
    }
    bool inserted = false;
    root = insertNode(root, c, inserted);
    if (inserted) {
        ++nodeCount;
    }
    return inserted;
}

// Recursive helper for insert. O(log n)
AVLTree::AVLNode* AVLTree::insertNode(AVLNode* n, const Camp& c, bool& inserted) {
    if (!n) {
        inserted = true;
        return new AVLNode{c, 1, nullptr, nullptr};
    }

    if (c.id < n->data.id) {
        // Recurse into left subtree
        n->left = insertNode(n->left, c, inserted);
    } else if (c.id > n->data.id) {
        // Recurse into right subtree
        n->right = insertNode(n->right, c, inserted);
    } else {
        // Duplicate id found
        inserted = false;
        return n;
    }

    if (!inserted) {
        return n;
    }

    return rebalance(n);
}

// Leftmost node in subtree, used to find in-order successor. O(log n)
AVLTree::AVLNode* AVLTree::findMin(AVLNode* n) {
    while (n && n->left) {
        n = n->left;
    }
    return n;
}

// Deletes camp by id and rebalances tree; false if not found. O(log n)
bool AVLTree::remove(int id) {
    rotations.clear();
    bool removed = false;
    root = removeNode(root, id, removed);
    if (removed) {
        --nodeCount;
    }
    return removed;
}

// Recursive helper for remove. O(log n)
AVLTree::AVLNode* AVLTree::removeNode(AVLNode* n, int id, bool& removed) {
    if (!n) {
        removed = false;
        return nullptr;
    }

    if (id < n->data.id) {
        // Recurse into left subtree
        n->left = removeNode(n->left, id, removed);
    } else if (id > n->data.id) {
        // Recurse into right subtree
        n->right = removeNode(n->right, id, removed);
    } else {
        // Found node to remove
        removed = true;

        // Leaf or single child case
        if (!n->left) {
            AVLNode* rightChild = n->right;
            delete n;
            return rightChild;
        } else if (!n->right) {
            AVLNode* leftChild = n->left;
            delete n;
            return leftChild;
        }

        // Two children: replace with in-order successor
        AVLNode* successor = findMin(n->right);
        n->data = successor->data;
        // Remove successor from right subtree
        n->right = removeNode(n->right, successor->data.id, removed);
    }

    if (!removed) {
        return n;
    }

    return rebalance(n);
}

// Copies the camp with the given id into out; false if not found. O(log n)
bool AVLTree::search(int id, Camp& out) const {
    AVLNode* curr = root;
    while (curr) {
        if (id < curr->data.id) {
            curr = curr->left;
        } else if (id > curr->data.id) {
            curr = curr->right;
        } else {
            out = curr->data;
            return true;
        }
    }
    return false;
}

// Updates name, x, y, and families for the given camp id; false if not found or invalid
// (empty name, negative families, or x/y not finite). O(log n)
bool AVLTree::update(int id, const std::string& name, double x, double y, int families) {
    if (name.empty() || families < 0 || !isValidCoordinate(x) || !isValidCoordinate(y)) {
        return false;
    }
    AVLNode* curr = root;
    while (curr) {
        if (id < curr->data.id) {
            curr = curr->left;
        } else if (id > curr->data.id) {
            curr = curr->right;
        } else {
            curr->data.name = name;
            curr->data.x = x;
            curr->data.y = y;
            curr->data.families = families;
            return true;
        }
    }
    return false;
}

// Adds amount to received total for resource r; false if not found or amount <= 0. O(log n)
bool AVLTree::addReceived(int id, ResourceType r, int amount) {
    if (amount <= 0) {
        return false;
    }
    AVLNode* curr = root;
    while (curr) {
        if (id < curr->data.id) {
            curr = curr->left;
        } else if (id > curr->data.id) {
            curr = curr->right;
        } else {
            switch (r) {
                case ResourceType::Food:
                    curr->data.receivedFood = saturatingAdd(curr->data.receivedFood, amount);
                    break;
                case ResourceType::Water:
                    curr->data.receivedWater = saturatingAdd(curr->data.receivedWater, amount);
                    break;
                case ResourceType::Medicine:
                    curr->data.receivedMedicine = saturatingAdd(curr->data.receivedMedicine, amount);
                    break;
            }
            return true;
        }
    }
    return false;
}

// Returns total number of camps stored in the tree. O(1)
int AVLTree::count() const {
    return nodeCount;
}

// Returns true if the tree has no camps. O(1)
bool AVLTree::isEmpty() const {
    return nodeCount == 0;
}

// Returns height of the root, or 0 if the tree is empty. O(1)
int AVLTree::treeHeight() const {
    return height(root);
}

// Helper for in-order traversal. O(n)
void AVLTree::inorderHelper(const AVLNode* n, std::vector<Camp>& out) {
    if (!n) return;
    inorderHelper(n->left, out);
    out.push_back(n->data);
    inorderHelper(n->right, out);
}

// Returns all camps ordered by ascending id. O(n)
std::vector<Camp> AVLTree::inorder() const {
    std::vector<Camp> result;
    if (nodeCount > 0) {
        result.reserve(nodeCount);
    }
    inorderHelper(root, result);
    return result;
}

// Helper for preorder snapshot. O(n)
void AVLTree::snapshotHelper(const AVLNode* n, int depth, char side, std::vector<AVLNodeInfo>& out) {
    if (!n) return;
    AVLNodeInfo info;
    info.id = n->data.id;
    info.height = n->height;
    info.balance = balanceFactor(const_cast<AVLNode*>(n));
    info.depth = depth;
    info.side = side;
    info.leftId = n->left ? n->left->data.id : 0;
    info.rightId = n->right ? n->right->data.id : 0;
    out.push_back(info);

    snapshotHelper(n->left, depth + 1, 'L', out);
    snapshotHelper(n->right, depth + 1, 'R', out);
}

// Populates out with all nodes in preorder (root, left, right). O(n)
void AVLTree::snapshot(std::vector<AVLNodeInfo>& out) const {
    out.clear();
    if (nodeCount > 0) {
        out.reserve(nodeCount);
    }
    snapshotHelper(root, 0, 'T', out);
}

// Returns rotations performed during the most recent insert or remove. O(1)
const std::vector<RotationEvent>& AVLTree::lastRotations() const {
    return rotations;
}

// Helper to recursively validate BST order, node heights, and balance factors. O(n)
bool AVLTree::validateAVL(const AVLNode* n, const AVLNode* minNode, const AVLNode* maxNode, int& computedHeight) {
    if (!n) {
        computedHeight = 0;
        return true;
    }

    // Check strict BST ordering
    if (minNode && n->data.id <= minNode->data.id) return false;
    if (maxNode && n->data.id >= maxNode->data.id) return false;

    int leftHeight = 0;
    int rightHeight = 0;

    if (!validateAVL(n->left, minNode, n, leftHeight)) return false;
    if (!validateAVL(n->right, n, maxNode, rightHeight)) return false;

    computedHeight = 1 + (leftHeight > rightHeight ? leftHeight : rightHeight);

    // Stored height must match computed height
    if (n->height != computedHeight) return false;

    // Balance factor must be -1, 0, or +1
    int bf = leftHeight - rightHeight;
    if (bf < -1 || bf > 1) return false;

    return true;
}

// Validates strict BST ordering, correct heights, and balance factors. O(n)
bool AVLTree::isValidAVL() const {
    int h = 0;
    return validateAVL(root, nullptr, nullptr, h);
}
