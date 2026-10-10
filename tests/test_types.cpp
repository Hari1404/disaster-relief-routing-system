#include "test_helpers.h"
#include "Types.h"

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
}
