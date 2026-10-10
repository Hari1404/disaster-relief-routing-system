#ifndef DELIVERY_LOG_H
#define DELIVERY_LOG_H

#include "Types.h"
#include <vector>

class DeliveryLog {
private:
    struct DeliveryNode {
        DeliveryRecord data;
        DeliveryNode* next;
    };

public:
    // Initialize empty log. O(1)
    DeliveryLog();
    // Destructor to free nodes. O(n)
    ~DeliveryLog();
    DeliveryLog(const DeliveryLog&) = delete;
    DeliveryLog& operator=(const DeliveryLog&) = delete;

    void append(const DeliveryRecord& rec);                      // O(1)
    int  size() const;                                           // O(1)
    bool isEmpty() const;                                        // O(1)
    std::vector<DeliveryRecord> toVector() const;                // oldest to newest, O(n)
    std::vector<DeliveryRecord> searchByCamp(int campId) const;  // oldest to newest, O(n)
    void clear();                                                // free all nodes, O(n)

private:
    DeliveryNode* head;
    DeliveryNode* tail;
    int count;
};

#endif // DELIVERY_LOG_H
