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
