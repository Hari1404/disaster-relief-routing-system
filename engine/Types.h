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

// Largest allowed map coordinate, in km (so |x| and |y| are at most 1,000,000 km).
// Keeping coordinates inside this range means no road length can overflow to infinity.
constexpr double kMaxCoordinateKm = 1.0e6;

// True only if v is a finite number with |v| <= kMaxCoordinateKm. Rejects NaN and infinity. O(1)
bool isValidCoordinate(double v);

// True only if: id >= 1, name not empty, x and y valid coordinates (see isValidCoordinate),
// families >= 0, all three received totals >= 0.
bool isValidCamp(const Camp& c);

// Parses s as a whole decimal number that fits in an int. Returns false (and leaves out unchanged)
// for an empty string, trailing characters ("12x"), or a value outside the int range, even when the
// platform's own `long` is only 32 bits (Windows). Used by the scenario loader and the console. O(length)
bool parseWholeNumber(const std::string& s, int& out);

#endif // TYPES_H

