#ifndef SORTING_H
#define SORTING_H

#include "Types.h"

// True if a should be served before b. Strict: higherPriority(a, a) is false.
// O(1) time complexity.
bool higherPriority(const Request& a, const Request& b);

enum class RankKey { Urgency, Families, Quantity, Arrival };

// The first key (in the order above) on which a and b differ.
// If they are identical on all four keys, return Arrival.
// O(1) time complexity.
RankKey rankReason(const Request& a, const Request& b);

// "urgency", "families", "quantity", "arrival"
// O(1) time complexity.
const char* rankKeyName(RankKey k);

// Hand-written, stable merge sort on a singly linked list of RequestNode.
// Returns the new head. Relinks the existing nodes; allocates no new nodes.
// O(n log n) time.
RequestNode* mergeSort(RequestNode* head);

// Merge two already-sorted lists into one sorted list.
// When neither node is higher priority than the other, take from the first list (stability).
// O(n) time complexity.
RequestNode* merge(RequestNode* a, RequestNode* b);

// Split a list into two halves using the slow/fast pointer method.
// Returns the head of the second half; the first half ends with next = nullptr.
// O(n) time complexity.
RequestNode* splitList(RequestNode* head);

#endif // SORTING_H
