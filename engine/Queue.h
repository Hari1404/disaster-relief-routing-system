#ifndef QUEUE_H
#define QUEUE_H

#include "Types.h"
#include <vector>

class Queue {
public:
    // Initialize empty queue. O(1)
    Queue();
    // Destructor to free nodes. O(n)
    ~Queue();
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    void enqueue(const Request& r);          // add at the back, O(1)
    bool dequeue(Request& out);              // remove from the front into out; false if empty, O(1)
    bool peek(Request& out) const;           // copy the front without removing; false if empty, O(1)
    bool isEmpty() const;                    // O(1)
    int  size() const;                       // O(1)
    int  removeByCamp(int campId);           // remove all requests for a camp; returns how many, O(n)
    std::vector<Request> toVector() const;   // front to back, O(n)
    void clear();                            // free all nodes, O(n)

private:
    RequestNode* head;
    RequestNode* tail;
    int count;
};

#endif // QUEUE_H
