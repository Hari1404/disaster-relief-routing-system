#include "Sorting.h"

// True if a should be served before b. Strict: higherPriority(a, a) is false.
// O(1) time complexity.
bool higherPriority(const Request& a, const Request& b) {
    if (a.urgency != b.urgency) return a.urgency > b.urgency;
    if (a.families != b.families) return a.families > b.families;
    if (a.quantity != b.quantity) return a.quantity > b.quantity;
    return a.arrivalSeq < b.arrivalSeq;
}

// The first key (in the order above) on which a and b differ.
// If they are identical on all four keys, return Arrival.
// O(1) time complexity.
RankKey rankReason(const Request& a, const Request& b) {
    if (a.urgency != b.urgency) return RankKey::Urgency;
    if (a.families != b.families) return RankKey::Families;
    if (a.quantity != b.quantity) return RankKey::Quantity;
    return RankKey::Arrival;
}

// "urgency", "families", "quantity", "arrival"
// O(1) time complexity.
const char* rankKeyName(RankKey k) {
    switch (k) {
        case RankKey::Urgency: return "urgency";
        case RankKey::Families: return "families";
        case RankKey::Quantity: return "quantity";
        case RankKey::Arrival: return "arrival";
    }
    return "arrival";
}

// Hand-written, stable merge sort on a singly linked list of RequestNode.
// Returns the new head. Relinks the existing nodes; allocates no new nodes.
// O(n log n) time.
RequestNode* mergeSort(RequestNode* head) {
    // Base case: 0 or 1 node
    if (!head || !head->next) {
        return head;
    }
    
    // Split the list into two halves
    RequestNode* secondHalf = splitList(head);
    
    // Recursively sort both halves
    head = mergeSort(head);
    secondHalf = mergeSort(secondHalf);
    
    // Merge the sorted halves
    return merge(head, secondHalf);
}

// Merge two already-sorted lists into one sorted list.
// When neither node is higher priority than the other, take from the first list (stability).
// This is done iteratively with a dummy head node.
// O(n) time complexity.
RequestNode* merge(RequestNode* a, RequestNode* b) {
    RequestNode dummy;
    dummy.data = Request{};
    dummy.next = nullptr;
    RequestNode* tail = &dummy;
    
    // Iterate while both lists have nodes
    while (a && b) {
        if (higherPriority(b->data, a->data)) {
            // b has strict higher priority
            tail->next = b; // append b
            b = b->next;    // advance b
        } else {
            // a has higher or equal priority (equal -> take from first list for stability)
            tail->next = a; // append a
            a = a->next;    // advance a
        }
        tail = tail->next;  // advance tail
    }
    
    // Append remaining nodes
    if (a) {
        tail->next = a;
    } else {
        tail->next = b;
    }
    
    return dummy.next;
}

// Split a list into two halves using the slow/fast pointer method.
// Returns the head of the second half; the first half ends with next = nullptr.
// O(n) time complexity.
RequestNode* splitList(RequestNode* head) {
    if (!head || !head->next) return nullptr;
    
    RequestNode* slow = head;
    RequestNode* fast = head->next;
    
    // Advance fast by two, slow by one
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    
    // slow is before the midpoint in the list
    RequestNode* secondHalf = slow->next;
    slow->next = nullptr; // break the list
    
    return secondHalf;
}
