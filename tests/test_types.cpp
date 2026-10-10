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
}
