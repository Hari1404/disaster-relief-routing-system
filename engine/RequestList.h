#ifndef REQUEST_LIST_H
#define REQUEST_LIST_H

#include "Types.h"
#include <vector>

class RequestList {
public:
    // Initialize empty list. O(1)
    RequestList();
    // Destructor to free all nodes. O(n)
    ~RequestList();
    RequestList(const RequestList&) = delete;
    RequestList& operator=(const RequestList&) = delete;

    bool insert(const Request& r);      // append at the tail; false (and no change) if r.id already exists, O(n)
    bool searchById(int id, Request& out) const;                 // linear search, O(n)
    int  searchByCamp(int campId, std::vector<Request>& out) const;  // appends matches in list order; returns count, O(n)
    bool update(int id, int urgency, int families, int quantity); // false if id not found OR any new value is invalid, O(n)
    bool deleteById(int id);            // false if not found, O(n)
    int  deleteByCamp(int campId);      // returns how many were removed, O(n)
    bool getTop(Request& out) const;    // copy of the head; false if empty, O(1)
    void sortByPriority();              // calls mergeSort, then re-finds the tail, O(n log n)
    bool isSortedByPriority() const;    // true if every neighbouring pair satisfies higherPriority(prev, next), O(n)
    bool isEmpty() const;               // O(1)
    int  size() const;                  // O(1)
    std::vector<Request> toVector() const;   // head to tail, O(n)
    void clear();                       // free all nodes, O(n)

private:
    RequestNode* head;
    RequestNode* tail;
    int count;
};

#endif // REQUEST_LIST_H
