#include "DeliveryLog.h"

// Initialize empty log. O(1)
DeliveryLog::DeliveryLog() : head(nullptr), tail(nullptr), count(0) {}

// Destructor to free nodes. O(n)
DeliveryLog::~DeliveryLog() {
    clear();
}

// Append record at the end, O(1)
void DeliveryLog::append(const DeliveryRecord& rec) {
    DeliveryNode* newNode = new DeliveryNode{rec, nullptr};
    if (!head) {
        head = newNode;
        tail = newNode;
    } else {
        tail->next = newNode;
        tail = newNode; // update tail
    }
    count++;
}

// Get the size of the log. O(1)
int DeliveryLog::size() const {
    return count;
}

// Check if log is empty. O(1)
bool DeliveryLog::isEmpty() const {
    return count == 0;
}

// Return all records oldest to newest, O(n)
std::vector<DeliveryRecord> DeliveryLog::toVector() const {
    std::vector<DeliveryRecord> vec;
    vec.reserve(count);
    DeliveryNode* current = head;
    while (current) { // traverse list
        vec.push_back(current->data);
        current = current->next;
    }
    return vec;
}

// Returns records for a specific camp, oldest to newest, O(n)
std::vector<DeliveryRecord> DeliveryLog::searchByCamp(int campId) const {
    std::vector<DeliveryRecord> vec;
    DeliveryNode* current = head;
    while (current) { // traverse list
        if (current->data.campId == campId) {
            vec.push_back(current->data);
        }
        current = current->next;
    }
    return vec;
}

// free all nodes, O(n)
void DeliveryLog::clear() {
    DeliveryNode* current = head;
    while (current) { // traverse list
        DeliveryNode* next = current->next;
        delete current; // free node
        current = next;
    }
    head = nullptr;
    tail = nullptr;
    count = 0;
}
