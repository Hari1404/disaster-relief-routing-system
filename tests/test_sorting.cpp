#include "test_helpers.h"
#include "Sorting.h"
#include "RequestList.h"
#include <vector>
#include <random>
#include <algorithm>
#include <cstring>

void runSortingTests() {
    // 1. higherPriority, one test per key
    Request base = makeRequest(1, 1, ResourceType::Food, 10, 2, 10, 10);
    
    // higher urgency wins
    Request rUrg = base; rUrg.urgency = 3; rUrg.arrivalSeq = 11;
    CHECK(higherPriority(rUrg, base));
    CHECK(!higherPriority(base, rUrg));
    
    // equal urgency, more families wins
    Request rFam = base; rFam.families = 20; rFam.arrivalSeq = 12;
    CHECK(higherPriority(rFam, base));
    
    // equal urgency and families, larger quantity wins
    Request rQty = base; rQty.quantity = 20; rQty.arrivalSeq = 13;
    CHECK(higherPriority(rQty, base));
    
    // all three equal, earlier arrivalSeq wins
    Request rArr = base; rArr.arrivalSeq = 5;
    CHECK(higherPriority(rArr, base));
    
    // 2. higherPriority(a, a) is false. Exactly one of (a,b) and (b,a) is true.
    CHECK(!higherPriority(base, base));
    CHECK(higherPriority(rUrg, base) ^ higherPriority(base, rUrg));
    
    // 3. Inversion case
    Request A = makeRequest(1, 1, ResourceType::Food, 900, 1, 500, 1);
    Request B = makeRequest(2, 2, ResourceType::Food, 5, 4, 10, 2);
    CHECK(higherPriority(B, A)); // B comes first because of urgency 4 vs 1
    
    // 4. Hand-checked fixture (the viva example)
    Request f1 = makeRequest(1, 1, ResourceType::Food, 50, 2, 30, 1);
    Request f2 = makeRequest(2, 2, ResourceType::Water, 200, 4, 10, 2);
    Request f3 = makeRequest(3, 3, ResourceType::Medicine, 20, 4, 40, 3);
    Request f4 = makeRequest(4, 1, ResourceType::Food, 80, 2, 30, 4);
    Request f5 = makeRequest(5, 4, ResourceType::Water, 500, 1, 200, 5);
    Request f6 = makeRequest(6, 2, ResourceType::Medicine, 20, 4, 40, 6);
    
    RequestList list;
    list.insert(f1); list.insert(f2); list.insert(f3);
    list.insert(f4); list.insert(f5); list.insert(f6);
    list.sortByPriority();
    
    auto v = list.toVector();
    CHECK(v.size() == 6);
    if(v.size() == 6) {
        CHECK(v[0].id == 3 && v[1].id == 6 && v[2].id == 2 &&
              v[3].id == 4 && v[4].id == 1 && v[5].id == 5);
    }
    
    list.clear();
    list.insert(f5); list.insert(f2); list.insert(f6);
    list.insert(f1); list.insert(f3); list.insert(f4);
    list.sortByPriority();
    v = list.toVector();
    if(v.size() == 6) {
        CHECK(v[0].id == 3 && v[1].id == 6 && v[2].id == 2 &&
              v[3].id == 4 && v[4].id == 1 && v[5].id == 5);
        
        CHECK(rankReason(v[0], v[1]) == RankKey::Arrival);
        CHECK(rankReason(v[1], v[2]) == RankKey::Families);
        CHECK(rankReason(v[2], v[3]) == RankKey::Urgency);
        CHECK(rankReason(v[3], v[4]) == RankKey::Quantity);
        CHECK(rankReason(v[4], v[5]) == RankKey::Urgency);
    }
    
    // 5. sortByPriority on: empty list, 1, 2 sorted, 2 reversed, sorted, reverse-sorted
    list.clear(); list.sortByPriority(); CHECK(list.isSortedByPriority() && list.size() == 0);
    list.insert(f1); list.sortByPriority(); CHECK(list.isSortedByPriority() && list.size() == 1);
    
    list.clear(); list.insert(f2); list.insert(f1); // f2 > f1
    list.sortByPriority(); CHECK(list.isSortedByPriority() && list.size() == 2);
    
    list.clear(); list.insert(f1); list.insert(f2); // reversed priority
    list.sortByPriority(); CHECK(list.isSortedByPriority() && list.size() == 2);
    
    list.clear();
    list.insert(f3); list.insert(f6); list.insert(f2);
    list.sortByPriority(); CHECK(list.isSortedByPriority() && list.size() == 3);
    
    // 6. Stability
    Request s1 = makeRequest(1, 1, ResourceType::Food, 10, 2, 10, 1);
    Request s2 = makeRequest(2, 1, ResourceType::Food, 10, 2, 10, 2);
    Request s3 = makeRequest(3, 1, ResourceType::Food, 10, 2, 10, 3);
    Request s4 = makeRequest(4, 1, ResourceType::Food, 10, 2, 10, 4);
    list.clear();
    list.insert(s3); list.insert(s1); list.insert(s4); list.insert(s2);
    list.sortByPriority();
    v = list.toVector();
    if(v.size() == 4) {
        CHECK(v[0].id == 1 && v[1].id == 2 && v[2].id == 3 && v[3].id == 4);
    }
    
    // 7. Randomised oracle
    int seeds[] = {42, 101, 999, 1234, 5678};
    for (int seed : seeds) {
        std::mt19937 gen(seed);
        std::uniform_int_distribution<> urg_dist(1, 4);
        std::uniform_int_distribution<> fam_dist(1, 50);
        std::uniform_int_distribution<> qty_dist(1, 100);
        
        list.clear();
        std::vector<Request> oracle;
        
        for (int i = 1; i <= 200; ++i) {
            Request r = makeRequest(i, 1, ResourceType::Food, qty_dist(gen), urg_dist(gen), fam_dist(gen), i);
            list.insert(r);
            oracle.push_back(r);
        }
        
        list.sortByPriority();
        std::sort(oracle.begin(), oracle.end(), higherPriority);
        
        v = list.toVector();
        bool match = true;
        for (size_t i = 0; i < 200; ++i) {
            if (v[i].id != oracle[i].id) {
                match = false;
                break;
            }
        }
        CHECK(match);
    }
    
    // 8. After sortByPriority, insert appends at the true end; isSortedByPriority becomes false if new request outranks tail
    list.clear();
    list.insert(f1); list.insert(f2);
    list.sortByPriority();
    // f2 then f1
    list.insert(f3); // f3 outranks f1 (tail)
    CHECK(!list.isSortedByPriority());
    list.sortByPriority();
    // tail is f1
    Request f7 = makeRequest(7, 1, ResourceType::Food, 1, 1, 1, 100);
    list.insert(f7); // f7 does not outrank f1
    CHECK(list.isSortedByPriority());
    
    // 9. isSortedByPriority is false for known wrong order
    list.clear();
    list.insert(f1); list.insert(f2); // f1 is lower priority than f2
    CHECK(!list.isSortedByPriority());
    
    // 10. rankKeyName returns exactly the required strings
    CHECK(std::strcmp(rankKeyName(RankKey::Urgency), "urgency") == 0);
    CHECK(std::strcmp(rankKeyName(RankKey::Families), "families") == 0);
    CHECK(std::strcmp(rankKeyName(RankKey::Quantity), "quantity") == 0);
    CHECK(std::strcmp(rankKeyName(RankKey::Arrival), "arrival") == 0);
}
