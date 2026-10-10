#include "test_helpers.h"
#include "DeliveryLog.h"

void runDeliveryLogTests() {
    DeliveryLog log;
    
    // 1. New log is empty, size 0.
    CHECK(log.isEmpty());
    CHECK(log.size() == 0);
    
    // 2. Append three records; size() is 3; toVector() is in append order.
    DeliveryRecord d1{1, 10, ResourceType::Food, 5, 5, 20, {0, 5, 10}};
    DeliveryRecord d2{2, 20, ResourceType::Water, 10, 10, 30, {0, 10, 20}};
    DeliveryRecord d3{3, 10, ResourceType::Medicine, 15, 15, 40, {0, 15, 10}};
    
    log.append(d1);
    log.append(d2);
    log.append(d3);
    
    CHECK(log.size() == 3);
    auto vec = log.toVector();
    CHECK(vec.size() == 3);
    if(vec.size() == 3) {
        CHECK(vec[0].requestId == 1);
        CHECK(vec[1].requestId == 2);
        CHECK(vec[2].requestId == 3);
    }
    
    // 3. searchByCamp returns only that camp's records, oldest to newest; empty result for unknown
    auto camp10 = log.searchByCamp(10);
    CHECK(camp10.size() == 2);
    if(camp10.size() == 2) {
        CHECK(camp10[0].requestId == 1);
        CHECK(camp10[1].requestId == 3);
    }
    
    auto camp99 = log.searchByCamp(99);
    CHECK(camp99.empty());
    
    // 4. The path vector survives intact
    CHECK(vec[0].path.size() == 3);
    if(vec[0].path.size() == 3) {
        CHECK(vec[0].path[0] == 0 && vec[0].path[1] == 5 && vec[0].path[2] == 10);
    }
    
    // 5. clear empties it and it can be reused
    log.clear();
    CHECK(log.isEmpty() && log.size() == 0);
    log.append(d1);
    CHECK(log.size() == 1);
}
