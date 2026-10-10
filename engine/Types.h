#ifndef TYPES_H
#define TYPES_H

#include <vector>

enum class ResourceType { Food = 0, Water = 1, Medicine = 2 };

struct Request {
    int          id;          // unique request ID, assigned by the caller
    int          campId;      // must be >= 1 (0 is reserved for the depot)
    ResourceType resource;
    int          quantity;    // units requested, > 0
    int          urgency;     // 1 = Low, 2 = Medium, 3 = High, 4 = Critical
    int          families;    // families affected, > 0
    long         arrivalSeq;  // arrival order number, >= 1, assigned by the caller
};

// One singly linked node holding a Request. Used by Queue and RequestList.
struct RequestNode {
    Request      data;
    RequestNode* next;
};

struct DeliveryRecord {
    int              requestId;
    int              campId;
    ResourceType     resource;
    int              quantityRequested;
    int              quantityDelivered;
    int              totalMinutes;   // route time in whole minutes
    std::vector<int> path;           // camp IDs from depot to camp, e.g. 0, 3, 5
};

// Returns true only if every field is within the ranges stated above.
// O(1) time complexity.
bool isValidRequest(const Request& r);

#include <string>

// A relief camp. The AVL tree is keyed by id.
struct Camp {
    int         id;                 // unique, >= 1 (0 is reserved for the depot)
    std::string name;               // not empty
    double      x;                  // position on the map, in km
    double      y;                  // position on the map, in km
    int         families;           // families living in the camp, >= 0
    int         receivedFood;       // total units delivered so far, >= 0
    int         receivedWater;
    int         receivedMedicine;
};

// True only if: id >= 1, name not empty, families >= 0, all three received totals >= 0.
bool isValidCamp(const Camp& c);

#endif // TYPES_H

