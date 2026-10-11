#include "Types.h"
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>

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

// True only if v is finite and within +/- kMaxCoordinateKm. The negated comparison also rejects NaN.
// O(1) time complexity.
bool isValidCoordinate(double v) {
    return std::isfinite(v) && std::fabs(v) <= kMaxCoordinateKm;
}

// True only if: id >= 1, name not empty, x and y valid coordinates, families >= 0, all three received totals >= 0.
// O(1) time complexity.
bool isValidCamp(const Camp& c) {
    if (c.id < 1) return false;
    if (c.name.empty()) return false;
    if (!isValidCoordinate(c.x) || !isValidCoordinate(c.y)) return false;   // NaN, infinity or too far away
    if (c.families < 0) return false;
    if (c.receivedFood < 0 || c.receivedWater < 0 || c.receivedMedicine < 0) return false;
    return true;
}

// Parses a whole number that fits in an int.
// strtoll is 64-bit on every platform; plain strtol is only 32-bit on Windows, where it silently
// saturates a huge value to INT_MAX and only reports it through errno. Check both.
bool parseWholeNumber(const std::string& s, int& out) {
    char* end = nullptr;
    errno = 0;
    long long v = std::strtoll(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return false;      // empty, or trailing characters
    if (errno == ERANGE || v < INT_MIN || v > INT_MAX) return false;
    out = static_cast<int>(v);
    return true;
}
