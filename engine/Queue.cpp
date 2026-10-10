#include "Queue.h"

// Initialize empty queue. O(1)
Queue::Queue() : head(nullptr), tail(nullptr), count(0) {}

// Destructor to free nodes. O(n)
Queue::~Queue() {
    clear();
}

// add at the back, O(1)
void Queue::enqueue(const Request& r) {
    RequestNode* newNode = new RequestNode{r, nullptr};
    if (!head) {
        // Queue was empty
        head = newNode;
        tail = newNode;
    } else {
        // Append to tail
        tail->next = newNode;
        tail = newNode; // update tail
    }
    count++;
}

// remove from the front into out; false if empty, O(1)
bool Queue::dequeue(Request& out) {
    if (!head) return false;
    
    RequestNode* oldHead = head;
    out = oldHead->data;
    
    // Move head forward
    head = head->next;
    if (!head) {
        // Queue became empty
        tail = nullptr; // update tail
    }
    
    delete oldHead; // free removed node
    count--;
    return true;
}

// copy the front without removing; false if empty, O(1)
bool Queue::peek(Request& out) const {
    if (!head) return false;
    out = head->data;
    return true;
}

// Check if queue is empty. O(1)
bool Queue::isEmpty() const {
    return count == 0;
}

// Get the size of the queue. O(1)
int Queue::size() const {
    return count;
}

// remove all requests for a camp; returns how many, O(n)
int Queue::removeByCamp(int campId) {
    int removedCount = 0;
    RequestNode* current = head;
    RequestNode* prev = nullptr;
    
    while (current) {
        if (current->data.campId == campId) {
            // Found a matching node to remove
            RequestNode* toDelete = current;
            if (prev) {
                // Node is in the middle or end
                prev->next = current->next;
            } else {
                // Node is at the head
                head = current->next;
            }
            
            // If the node was the tail, update tail
            if (current == tail) {
                tail = prev; // new tail could be null
            }
            
            current = current->next; // advance current
            delete toDelete; // free removed node
            removedCount++;
            count--;
        } else {
            // Move forward
            prev = current;
            current = current->next;
        }
    }
    
    return removedCount;
}

// front to back, O(n)
std::vector<Request> Queue::toVector() const {
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
void Queue::clear() {
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
