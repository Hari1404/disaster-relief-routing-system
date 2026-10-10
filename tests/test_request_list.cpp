#include "test_helpers.h"
#include "RequestList.h"

void runRequestListTests() {
    RequestList list;
    Request out;
    
    // 1. Insert three; size() is 3; searchById finds each and misses an unknown ID
    Request r1 = makeRequest(1, 10, ResourceType::Food, 5, 2, 10, 1);
    Request r2 = makeRequest(2, 20, ResourceType::Water, 10, 3, 20, 2);
    Request r3 = makeRequest(3, 30, ResourceType::Medicine, 15, 4, 30, 3);
    
    CHECK(list.insert(r1));
    CHECK(list.insert(r2));
    CHECK(list.insert(r3));
    CHECK(list.size() == 3);
    
    CHECK(list.searchById(1, out) && out.id == 1);
    CHECK(list.searchById(2, out) && out.id == 2);
    CHECK(list.searchById(3, out) && out.id == 3);
    CHECK(!list.searchById(99, out));
    
    // 2. Inserting a duplicate ID returns false and leaves the size unchanged
    CHECK(!list.insert(makeRequest(2, 40, ResourceType::Food, 1, 1, 1, 4)));
    CHECK(list.size() == 3);
    
    // 3. searchByCamp returns all matches in list order and returns count; returns 0 for none
    list.insert(makeRequest(4, 20, ResourceType::Food, 1, 1, 1, 5));
    std::vector<Request> camp20;
    CHECK(list.searchByCamp(20, camp20) == 2);
    CHECK(camp20.size() == 2 && camp20[0].id == 2 && camp20[1].id == 4);
    
    std::vector<Request> camp99;
    CHECK(list.searchByCamp(99, camp99) == 0);
    CHECK(camp99.empty());
    
    // 4. update changes the three fields; other fields stay the same
    CHECK(list.update(2, 1, 50, 100));
    list.searchById(2, out);
    CHECK(out.urgency == 1 && out.families == 50 && out.quantity == 100);
    CHECK(out.campId == 20 && out.resource == ResourceType::Water && out.arrivalSeq == 2);
    
    // 5. update rejects urgency=0, urgency=5, families=0, quantity=0, and unknown ID, leaving record unchanged
    CHECK(!list.update(2, 0, 50, 100));
    CHECK(!list.update(2, 5, 50, 100));
    CHECK(!list.update(2, 1, 0, 100));
    CHECK(!list.update(2, 1, 50, 0));
    CHECK(!list.update(99, 1, 50, 100));
    list.searchById(2, out);
    CHECK(out.urgency == 1 && out.families == 50 && out.quantity == 100);
    
    // 6. deleteById: only element, head, middle, tail, and missing ID
    list.clear();
    // only element
    list.insert(r1);
    CHECK(list.deleteById(1));
    CHECK(list.size() == 0);
    
    // head
    list.insert(r1); list.insert(r2); list.insert(r3);
    CHECK(list.deleteById(1));
    CHECK(list.size() == 2);
    
    // middle
    list.clear(); list.insert(r1); list.insert(r2); list.insert(r3);
    CHECK(list.deleteById(2));
    CHECK(list.size() == 2);
    
    // tail
    list.clear(); list.insert(r1); list.insert(r2); list.insert(r3);
    CHECK(list.deleteById(3));
    CHECK(list.size() == 2);
    
    // missing
    CHECK(!list.deleteById(99));
    
    // 7. After deleting the tail, insert appends correctly
    list.clear(); list.insert(r1); list.insert(r2);
    list.deleteById(2);
    list.insert(r3);
    auto vec = list.toVector();
    CHECK(vec.size() == 2 && vec[0].id == 1 && vec[1].id == 3);
    
    // 8. deleteByCamp: returns right count for some, all, and none
    list.clear();
    list.insert(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    list.insert(makeRequest(2, 20, ResourceType::Food, 1, 1, 1, 2));
    list.insert(makeRequest(3, 10, ResourceType::Food, 1, 1, 1, 3));
    CHECK(list.deleteByCamp(10) == 2);
    CHECK(list.size() == 1);
    
    list.clear();
    list.insert(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    list.insert(makeRequest(2, 10, ResourceType::Food, 1, 1, 1, 2));
    CHECK(list.deleteByCamp(10) == 2);
    CHECK(list.size() == 0);
    
    list.clear();
    list.insert(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    CHECK(list.deleteByCamp(99) == 0);
    CHECK(list.size() == 1);
    
    // 9. getTop is false on empty list; true after inserts (returns head)
    list.clear();
    CHECK(!list.getTop(out));
    list.insert(r1);
    list.insert(r2);
    CHECK(list.getTop(out) && out.id == 1);
    
    // 10. clear empties and the list is reusable
    list.clear();
    CHECK(list.isEmpty() && list.size() == 0);
    list.insert(r1);
    CHECK(list.size() == 1);
}
