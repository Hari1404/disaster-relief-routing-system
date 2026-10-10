#include "Types.h"

// Returns true only if every field is within the ranges stated above.
// O(1) time complexity.
bool isValidRequest(const Request& r) {
    if (r.id <= 0) return false;
    if (r.campId < 1) return false;
    if (r.quantity <= 0) return false;
    if (r.families <= 0) return false;
    if (r.urgency < 1 || r.urgency > 4) return false;
    if (r.arrivalSeq < 1) return false;
    if (r.resource != ResourceType::Food && r.resource != ResourceType::Water && r.resource != ResourceType::Medicine) return false;
    return true;
}

// True only if: id >= 1, name not empty, families >= 0, all three received totals >= 0.
// O(1) time complexity.
bool isValidCamp(const Camp& c) {
    if (c.id < 1) return false;
    if (c.name.empty()) return false;
    if (c.families < 0) return false;
    if (c.receivedFood < 0 || c.receivedWater < 0 || c.receivedMedicine < 0) return false;
    return true;
}

