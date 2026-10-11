#include "test_helpers.h"
#include "Types.h"
#include <limits>

void runTypesTests() {
    // A valid request passes
    Request valid = makeRequest(1, 1, ResourceType::Food, 10, 2, 5, 1);
    CHECK(isValidRequest(valid));

    // Each field failing alone makes isValidRequest false
    Request r1 = valid; r1.id = 0; CHECK(!isValidRequest(r1));
    Request r2 = valid; r2.campId = 0; CHECK(!isValidRequest(r2));
    Request r3 = valid; r3.quantity = 0; CHECK(!isValidRequest(r3));
    Request r4 = valid; r4.families = 0; CHECK(!isValidRequest(r4));
    Request r5 = valid; r5.urgency = 0; CHECK(!isValidRequest(r5));
    Request r6 = valid; r6.urgency = 5; CHECK(!isValidRequest(r6));
    Request r7 = valid; r7.arrivalSeq = 0; CHECK(!isValidRequest(r7));

    // Out-of-range resource value
    Request r8 = valid; r8.resource = static_cast<ResourceType>(9); CHECK(!isValidRequest(r8));

    // Camps: a valid one passes, and each bad field fails on its own
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    Camp camp = {1, "Riverside", 3.0, 4.0, 120, 0, 0, 0};
    CHECK(isValidCamp(camp));
    Camp k1 = camp; k1.id = 0;          CHECK(!isValidCamp(k1));
    Camp k2 = camp; k2.name = "";       CHECK(!isValidCamp(k2));
    Camp k3 = camp; k3.families = -1;   CHECK(!isValidCamp(k3));
    Camp k4 = camp; k4.receivedWater = -1; CHECK(!isValidCamp(k4));

    // x and y must be finite numbers
    Camp k5 = camp; k5.x = nan;   CHECK(!isValidCamp(k5));
    Camp k6 = camp; k6.y = nan;   CHECK(!isValidCamp(k6));
    Camp k7 = camp; k7.x = inf;   CHECK(!isValidCamp(k7));
    Camp k8 = camp; k8.y = -inf;  CHECK(!isValidCamp(k8));
    Camp k9 = camp; k9.x = -3.5; k9.y = 0.0; CHECK(isValidCamp(k9));   // negative and zero are fine

    // Coordinates are limited to +/-1,000,000 km (BUG-2): too far away is invalid, like NaN
    CHECK(isValidCoordinate(0.0));
    CHECK(isValidCoordinate(kMaxCoordinateKm));
    CHECK(isValidCoordinate(-kMaxCoordinateKm));
    CHECK(!isValidCoordinate(kMaxCoordinateKm + 1.0));
    CHECK(!isValidCoordinate(-kMaxCoordinateKm - 1.0));
    CHECK(!isValidCoordinate(1e12));
    CHECK(!isValidCoordinate(1e200));
    CHECK(!isValidCoordinate(nan));
    CHECK(!isValidCoordinate(inf));
    Camp far1 = camp; far1.x = 1e200;                 CHECK(!isValidCamp(far1));
    Camp far2 = camp; far2.y = -2e6;                  CHECK(!isValidCamp(far2));
    Camp edge = camp; edge.x = 1e6; edge.y = -1e6;    CHECK(isValidCamp(edge));

    // parseWholeNumber: used by the scenario loader and the console (BUG-1 and the Windows strtol problem)
    int n = 777;
    CHECK(parseWholeNumber("42", n) && n == 42);
    CHECK(parseWholeNumber("0", n) && n == 0);
    CHECK(parseWholeNumber("-7", n) && n == -7);
    CHECK(parseWholeNumber("2147483647", n) && n == 2147483647);            // INT_MAX
    CHECK(parseWholeNumber("-2147483648", n) && n == -2147483647 - 1);      // INT_MIN
    n = 777;
    CHECK(!parseWholeNumber("", n) && n == 777);                            // a failure leaves out unchanged
    CHECK(!parseWholeNumber("x", n) && n == 777);
    CHECK(!parseWholeNumber("12x", n) && n == 777);                         // trailing junk
    CHECK(!parseWholeNumber("1.5", n) && n == 777);
    CHECK(!parseWholeNumber("2147483648", n) && n == 777);                  // INT_MAX + 1
    CHECK(!parseWholeNumber("-2147483649", n) && n == 777);                 // INT_MIN - 1
    CHECK(!parseWholeNumber("4294967297", n) && n == 777);                  // 2^32 + 1 must not wrap to 1
    CHECK(!parseWholeNumber("99999999999", n) && n == 777);
    CHECK(!parseWholeNumber("99999999999999999999", n) && n == 777);        // beyond even 64 bits
}
