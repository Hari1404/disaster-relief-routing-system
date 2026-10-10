#include "RequestList.h"
#include "Sorting.h"

// Initialize empty list. O(1)
RequestList::RequestList() : head(nullptr), tail(nullptr), count(0) {}

// Destructor to free all nodes. O(n)
RequestList::~RequestList() {
    clear();
}

// append at the tail; false (and no change) if r.id already exists, O(n)
bool RequestList::insert(const Request& r) {
    Request dummy;
    if (searchById(r.id, dummy)) {
        return false;
    }
    
    RequestNode* newNode = new RequestNode{r, nullptr};
    if (!head) {
        // List was empty
        head = newNode;
        tail = newNode;
    } else {
        // Append at tail
        tail->next = newNode;
        tail = newNode; // update tail
    }
    count++;
    return true;
}

// linear search, O(n)
bool RequestList::searchById(int id, Request& out) const {
    RequestNode* current = head;
    while (current) { // traverse list
        if (current->data.id == id) {
            out = current->data;
            return true;
        }
        current = current->next; // advance current
    }
    return false;
}

// appends matches in list order; returns count, O(n)
int RequestList::searchByCamp(int campId, std::vector<Request>& out) const {
    int matches = 0;
    RequestNode* current = head;
    while (current) { // traverse list
        if (current->data.campId == campId) {
            out.push_back(current->data);
            matches++;
        }
        current = current->next; // advance current
    }
    return matches;
}

// false if id not found OR any new value is invalid, O(n)
bool RequestList::update(int id, int urgency, int families, int quantity) {
    if (urgency < 1 || urgency > 4) return false;
    if (families <= 0) return false;
    if (quantity <= 0) return false;
    
    RequestNode* current = head;
    while (current) { // traverse list
        if (current->data.id == id) {
            current->data.urgency = urgency;
            current->data.families = families;
            current->data.quantity = quantity;
            return true;
        }
        current = current->next; // advance current
    }
    return false;
}

// false if not found, O(n)
bool RequestList::deleteById(int id) {
    RequestNode* current = head;
    RequestNode* prev = nullptr;
    
    while (current) {
        if (current->data.id == id) {
            // Found node to delete
            if (prev) {
                // Middle or tail
                prev->next = current->next;
            } else {
                // Head
                head = current->next;
            }
            
            // Update tail if deleting the tail
            if (current == tail) {
                tail = prev;
            }
            
            delete current; // free node
            count--;
            return true;
        }
        prev = current;
        current = current->next;
    }
    
    return false;
}

// returns how many were removed, O(n)
int RequestList::deleteByCamp(int campId) {
    int removedCount = 0;
    RequestNode* current = head;
    RequestNode* prev = nullptr;
    
    while (current) {
        if (current->data.campId == campId) {
            // Delete current node
            RequestNode* toDelete = current;
            if (prev) {
                prev->next = current->next;
            } else {
                head = current->next;
            }
            
            if (current == tail) {
                tail = prev; // update tail
            }
            
            current = current->next; // advance current
            delete toDelete; // free node
            removedCount++;
            count--;
        } else {
            // Advance
            prev = current;
            current = current->next;
        }
    }
    
    return removedCount;
}

// copy of the head; false if empty, O(1)
bool RequestList::getTop(Request& out) const {
    if (!head) return false;
    out = head->data;
    return true;
}

// calls mergeSort, then re-finds the tail, O(n log n)
void RequestList::sortByPriority() {
    if (!head || !head->next) return;
    
    head = mergeSort(head);
    
    // Re-find tail
    RequestNode* current = head;
    while (current && current->next) { // traverse to end
        current = current->next;
    }
    tail = current; // update tail
}

// true if every neighbouring pair satisfies higherPriority(prev, next), O(n)
bool RequestList::isSortedByPriority() const {
    if (!head || !head->next) return true;
    
    RequestNode* current = head;
    while (current->next) { // check pairs
        if (!higherPriority(current->data, current->next->data)) {
            return false;
        }
        current = current->next;
    }
    return true;
}

// Check if list is empty, O(1)
bool RequestList::isEmpty() const {
    return count == 0;
}

// Get the size of the list, O(1)
int RequestList::size() const {
    return count;
}

// head to tail, O(n)
std::vector<Request> RequestList::toVector() const {
    std::vector<Request> vec;
    vec.reserve(count);
    RequestNode* current = head;
    while (current) { // traverse list
        vec.push_back(current->data);
        current = current->next; // advance current
    }
    return vec;
}

// free all nodes, O(n)
void RequestList::clear() {
    RequestNode* current = head;
    while (current) { // traverse list
        RequestNode* next = current->next; // save next
        delete current; // free node
        current = next; // advance current
    }
    head = nullptr;
    tail = nullptr;
    count = 0;
}
