#include "test_helpers.h"
#include "../engine/Engine.h"
#include <limits>

void runEngineTests() {
    auto setupMiniWorld = [](Engine& e) {
        Camp c1 = {1, "Camp1", 3.0, 4.0, 100, 0, 0, 0};
        e.addCamp(c1, {0});
        Camp c2 = {2, "Camp2", 6.0, 8.0, 100, 0, 0, 0};
        e.addCamp(c2, {1});
    };

    {
        // 1. Fresh engine and reset
        Engine e;
        CHECK(e.campCount() == 0);
        CHECK(e.queueContents().empty());
        CHECK(e.stock().food == 500 && e.stock().water == 500 && e.stock().medicine == 500);
        CHECK(e.averageSpeed() == 40.0);
        CHECK(e.depotX() == 0.0);
        
        setupMiniWorld(e);
        CHECK(e.campCount() == 2);
        
        e.reset();
        CHECK(e.campCount() == 0);
        CHECK(e.queueContents().empty());
        
        e.addCamp({1, "C", 1, 1, 10, 0, 0, 0}, {0});
        auto res = e.submitRequest(1, ResourceType::Food, 10, 1, 10);
        CHECK(res.value == 1);
    }
    {
        // 2. addCamp
        Engine e;
        e.setAverageSpeed(60.0);
        Camp c1 = {1, "Camp1", 3.0, 4.0, 100, 0, 0, 0};
        auto res = e.addCamp(c1, {0});
        CHECK(res.ok() && res.value == 1);
        
        Camp c2 = {2, "Camp2", 6.0, 8.0, 100, 0, 0, 0};
        res = e.addCamp(c2, {1});
        CHECK(res.ok() && res.value == 1);
        
        auto routes = e.routes();
        CHECK(routes.size() == 2);
        if (routes.size() == 2) {
            CHECK(routes[0].campId == 1 && routes[0].minutes == 5 && routes[0].path.size() == 2);
            CHECK(routes[1].campId == 2 && routes[1].minutes == 10 && routes[1].path.size() == 3);
        }
        
        CHECK(e.addCamp(c1, {0}).err == Err::Duplicate);
        Camp c0 = {0, "C", 1, 1, 10, 0, 0, 0};
        CHECK(e.addCamp(c0, {0}).err == Err::Invalid); // id 0
        Camp cEmpty = {3, "", 1, 1, 10, 0, 0, 0};
        CHECK(e.addCamp(cEmpty, {0}).err == Err::Invalid);
        Camp cNegF = {3, "C", 1, 1, -1, 0, 0, 0};
        CHECK(e.addCamp(cNegF, {0}).err == Err::Invalid);
        Camp c3 = {3, "Camp3", 1.0, 1.0, 100, 0, 0, 0};
        CHECK(e.addCamp(c3, {3}).err == Err::Invalid); // link to self
        CHECK(e.addCamp(c3, {1, 1}).err == Err::Invalid); // dup link
        Camp c4 = {4, "Camp4", 0.0, 0.0, 100, 0, 0, 0};
        CHECK(e.addCamp(c4, {0}).err == Err::Invalid); // same pos
        CHECK(e.addCamp(c3, {99}).err == Err::NotFound);
        
        CHECK(e.campCount() == 2);
        CHECK(e.roads().size() == 2); // undirected count
    }
    {
        // 3. addCamp trace
        Engine e;
        Camp c10 = {10, "C", 1, 1, 10, 0, 0, 0};
        e.addCamp(c10, {0});
        CHECK(e.lastTrace().find("AVL insert 10: no rotation") == 0);
        
        Camp c20 = {20, "C", 2, 2, 10, 0, 0, 0};
        e.addCamp(c20, {0});
        CHECK(e.lastTrace().find("AVL insert 20: no rotation") == 0);
        
        Camp c30 = {30, "C", 3, 3, 10, 0, 0, 0};
        e.addCamp(c30, {0});
        CHECK(e.lastTrace().find("AVL insert 30 caused RR rotation at node 10") == 0);
        CHECK(e.lastRotations().size() == 1);
        if (!e.lastRotations().empty()) {
            CHECK(e.lastRotations()[0].type == RotationType::RR);
            CHECK(e.lastRotations()[0].nodeId == 10);
        }
    }
    {
        // 4. editCamp
        Engine e;
        e.setAverageSpeed(60.0);
        setupMiniWorld(e);
        
        CHECK(e.editCamp(2, "NewName", 9.0, 12.0, 200).ok());
        Camp c;
        CHECK(e.findCamp(2, c));
        CHECK(c.name == "NewName" && c.families == 200 && c.x == 9.0 && c.y == 12.0);
        
        auto routes = e.routes();
        CHECK(routes.size() == 2);
        if (routes.size() == 2) {
            CHECK(routes[1].campId == 2 && routes[1].minutes == 15);
        }
        
        CHECK(e.editCamp(2, "C", 3.0, 4.0, 100).err == Err::Invalid);
        CHECK(e.editCamp(99, "C", 1, 1, 1).err == Err::NotFound);
        CHECK(e.editCamp(2, "", 1, 1, 1).err == Err::Invalid);
    }
    {
        // 5. Roads
        Engine e;
        e.setAverageSpeed(60.0);
        setupMiniWorld(e);
        
        e.setRoadStatus(0, 1, RoadStatus::Blocked);
        auto routes = e.routes();
        if (routes.size() == 2) {
            CHECK(!routes[0].reachable);
            CHECK(!routes[1].reachable);
        }
        
        CHECK(e.addRoad(0, 2).ok());
        routes = e.routes();
        if (routes.size() == 2) {
            CHECK(routes[1].reachable && routes[1].minutes == 10 && routes[1].path.size() == 2);
            CHECK(routes[0].reachable && routes[0].minutes == 15 && routes[0].path.size() == 3);
        }
        
        CHECK(e.addRoad(0, 2).err == Err::Duplicate);
        CHECK(e.addRoad(1, 1).err == Err::Invalid);
        CHECK(e.addRoad(0, 99).err == Err::NotFound);
        CHECK(e.setRoadStatus(1, 5, RoadStatus::Blocked).err == Err::NotFound);
        CHECK(e.removeRoad(1, 5).err == Err::NotFound);
        CHECK(e.removeRoad(0, 2).ok());
        
        e.setAverageSpeed(30.0);
        e.setRoadStatus(0, 1, RoadStatus::Open);
        routes = e.routes();
        if (routes.size() == 2) {
            CHECK(routes[0].minutes == 10); // 5km at 30km/h
            CHECK(routes[1].minutes == 20); // 10km at 30km/h
        }
        CHECK(e.setAverageSpeed(0.0).err == Err::Invalid);
    }
    {
        // 6. submitRequest
        Engine e;
        setupMiniWorld(e);
        CHECK(e.submitRequest(99, ResourceType::Food, 10, 1, 1).err == Err::NotFound);
        CHECK(e.submitRequest(1, ResourceType::Food, 0, 1, 1).err == Err::Invalid);
        CHECK(e.submitRequest(1, ResourceType::Food, 10, 5, 1).err == Err::Invalid);
        CHECK(e.submitRequest(1, ResourceType::Food, 10, 1, 0).err == Err::Invalid);
        
        auto res = e.submitRequest(1, ResourceType::Food, 100, 2, 10);
        CHECK(res.ok() && res.value == 1);
        auto q = e.queueContents();
        CHECK(q.size() == 1);
        if (q.size() == 1) {
            CHECK(q[0].id == 1 && q[0].arrivalSeq == 1);
        }
        
        res = e.submitRequest(2, ResourceType::Water, 50, 4, 5);
        CHECK(res.ok() && res.value == 2);
        q = e.queueContents();
        CHECK(q.size() == 2);
        if (q.size() == 2) {
            CHECK(q[1].id == 2 && q[1].arrivalSeq == 2);
        }
    }
    {
        // 7. processIncoming
        Engine e;
        setupMiniWorld(e);
        CHECK(e.processIncoming().err == Err::Empty);
        
        e.submitRequest(1, ResourceType::Food, 100, 2, 10); // id 1
        e.submitRequest(2, ResourceType::Water, 50, 4, 5);  // id 2
        
        auto res = e.processIncoming();
        CHECK(res.ok() && res.value == 2);
        CHECK(e.queueContents().empty());
        
        auto ranked = e.rankedRequests();
        CHECK(ranked.size() == 2);
        if (ranked.size() == 2) {
            CHECK(ranked[0].req.id == 2);
            CHECK(ranked[1].req.id == 1);
            CHECK(ranked[0].hasReason && ranked[0].reason == RankKey::Urgency);
            CHECK(!ranked[1].hasReason);
        }
    }
    {
        // 8. updateRequest
        Engine e;
        setupMiniWorld(e);
        e.submitRequest(1, ResourceType::Food, 100, 2, 10); // id 1
        e.submitRequest(2, ResourceType::Water, 50, 4, 5);  // id 2
        CHECK(e.updateRequest(1, 4, 50, 100).err == Err::NotFound);
        
        e.processIncoming();
        CHECK(e.updateRequest(99, 4, 50, 100).err == Err::NotFound);
        CHECK(e.updateRequest(1, 0, 50, 100).err == Err::Invalid);
        
        CHECK(e.updateRequest(1, 4, 50, 100).ok());
        auto ranked = e.rankedRequests();
        CHECK(ranked.size() == 2);
        if (ranked.size() == 2) {
            CHECK(ranked[0].req.id == 1); // Now urgency 4, families 50 vs urgency 4, families 5
        }
    }
    {
        // 9. handleNext, full delivery
        Engine e;
        e.setAverageSpeed(60.0);
        setupMiniWorld(e);
        e.submitRequest(1, ResourceType::Food, 100, 2, 10);
        e.submitRequest(2, ResourceType::Water, 50, 4, 5);
        e.processIncoming();
        
        auto res = e.handleNext();
        CHECK(res.ok() && res.value == 50);
        CHECK(e.stock().water == 450);
        
        Camp c2;
        e.findCamp(2, c2);
        CHECK(c2.receivedWater == 50);
        
        auto log = e.deliveries();
        CHECK(log.size() == 1);
        if (log.size() == 1) {
            CHECK(log[0].requestId == 2 && log[0].campId == 2 && log[0].resource == ResourceType::Water);
            CHECK(log[0].quantityRequested == 50 && log[0].quantityDelivered == 50 && log[0].totalMinutes == 10);
            CHECK(log[0].path.size() == 3);
        }
        
        CHECK(e.rankedRequests().size() == 1);
        
        res = e.handleNext();
        CHECK(res.ok() && res.value == 100);
        
        CHECK(e.handleNext().err == Err::Empty);
    }
    {
        // 10. handleNext, partial and out of stock
        Engine e;
        e.setAverageSpeed(60.0);
        setupMiniWorld(e);
        e.setStock(30, 500, 500);
        e.submitRequest(1, ResourceType::Food, 100, 2, 10);
        e.processIncoming();
        
        auto res = e.handleNext();
        CHECK(res.ok() && res.value == 30);
        CHECK(e.stock().food == 0);
        
        auto log = e.deliveries();
        CHECK(log.size() == 1);
        if (log.size() == 1) {
            CHECK(log[0].quantityRequested == 100 && log[0].quantityDelivered == 30);
        }
        
        auto ranked = e.rankedRequests();
        CHECK(ranked.size() == 1);
        if (ranked.size() == 1) {
            CHECK(ranked[0].req.quantity == 70);
        }
        
        CHECK(e.handleNext().err == Err::OutOfStock);
        CHECK(e.rankedRequests().size() == 1);
    }
    {
        // 11. handleNext skips unreachable
        Engine e;
        e.setAverageSpeed(60.0);
        setupMiniWorld(e);
        Camp c3 = {3, "Camp3", 20.0, 20.0, 100, 0, 0, 0};
        e.addCamp(c3, {});
        
        e.submitRequest(3, ResourceType::Food, 10, 4, 10); // id 1
        e.submitRequest(1, ResourceType::Food, 10, 1, 10); // id 2
        e.processIncoming();
        
        auto ranked = e.rankedRequests();
        CHECK(ranked.size() == 2);
        if (ranked.size() == 2) {
            CHECK(ranked[0].req.id == 1);
        }
        
        auto res = e.handleNext();
        CHECK(res.ok() && res.value == 10);
        
        auto log = e.deliveries();
        CHECK(log.size() == 1);
        if (log.size() == 1) {
            CHECK(log[0].requestId == 2);
        }
        
        ranked = e.rankedRequests();
        CHECK(ranked.size() == 1 && ranked[0].req.id == 1);
        
        res = e.handleNext();
        CHECK(res.err == Err::Unreachable);
        CHECK(res.message.find("Unreachable: alternate delivery needed") == 0);
    }

    {
        // 12. deleteCamp better
        Engine e;
        setupMiniWorld(e);
        e.submitRequest(1, ResourceType::Food, 10, 1, 10);
        e.processIncoming();
        e.submitRequest(1, ResourceType::Water, 10, 2, 10);
        
        CHECK(e.deleteCamp(1, false).err == Err::ConfirmNeeded);
        CHECK(e.deleteCamp(1, false).value == 2);
        
        auto res = e.deleteCamp(1, true);
        CHECK(res.ok() && res.value == 2);
        
        Camp c;
        CHECK(!e.findCamp(1, c));
        CHECK(e.queueContents().empty());
        CHECK(e.rankedRequests().empty());
        CHECK(e.roads().size() == 0);
        CHECK(e.lastTrace().find("AVL delete 1") == 0);
        
        CHECK(e.deleteCamp(0, true).err == Err::Invalid);
        CHECK(e.deleteCamp(99, true).err == Err::NotFound);
    }
    {
        // 13. Counters and 14. Trace
        Engine e;
        e.submitRequest(1, ResourceType::Food, 10, 1, 10); // fails
        auto res = e.submitRequest(1, ResourceType::Food, 10, 1, 10);
        CHECK(res.err == Err::NotFound);
        
        setupMiniWorld(e);
        res = e.submitRequest(1, ResourceType::Food, 10, 1, 10);
        CHECK(res.value == 1);
        CHECK(!e.lastTrace().empty());
    }
    {
        // 15. Non-finite numbers are rejected and change nothing
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        Engine e;
        Camp badX = {1, "Bad", nan, 1.0, 10, 0, 0, 0};
        Camp badY = {1, "Bad", 1.0, inf, 10, 0, 0, 0};
        CHECK(e.addCamp(badX, {0}).err == Err::Invalid);
        CHECK(e.addCamp(badY, {0}).err == Err::Invalid);
        CHECK(e.campCount() == 0);
        CHECK(e.roads().empty());

        setupMiniWorld(e);   // camp 1 at (3, 4), camp 2 at (6, 8), depot at (0, 0)
        CHECK(e.editCamp(1, "C", nan, 4.0, 10).err == Err::Invalid);
        CHECK(e.editCamp(1, "C", 3.0, -inf, 10).err == Err::Invalid);
        Camp c;
        CHECK(e.findCamp(1, c) && c.x == 3.0 && c.y == 4.0 && c.name == "Camp1");

        CHECK(e.setDepotPosition(nan, 0.0).err == Err::Invalid);
        CHECK(e.setDepotPosition(0.0, inf).err == Err::Invalid);
        CHECK(e.depotX() == 0.0 && e.depotY() == 0.0);

        CHECK(e.setAverageSpeed(nan).err == Err::Invalid);
        CHECK(e.setAverageSpeed(inf).err == Err::Invalid);
        CHECK(e.averageSpeed() == 40.0);
    }
    {
        // 16. An absurdly slow speed gives clamped travel times, not overflow garbage
        Engine e;
        setupMiniWorld(e);   // depot - camp 1 - camp 2, two roads of 5 km each
        CHECK(e.setAverageSpeed(1e-12).ok());
        auto routes = e.routes();
        CHECK(routes.size() == 2);
        if (routes.size() == 2) {
            CHECK(routes[0].reachable && routes[0].minutes == Graph::kMaxRoadMinutes);
            CHECK(routes[1].reachable && routes[1].minutes == 2 * Graph::kMaxRoadMinutes);
        }
    }
    {
        // 17. handleNext skips a request whose resource is out of stock
        Engine e;
        e.setAverageSpeed(60.0);
        setupMiniWorld(e);
        e.setStock(0, 100, 100);
        e.submitRequest(1, ResourceType::Food, 10, 4, 10);    // id 1: ranked first, but food is 0
        e.submitRequest(2, ResourceType::Water, 10, 1, 10);   // id 2: ranked second, water in stock
        e.processIncoming();
        auto before = e.rankedRequests();
        CHECK(before.size() == 2 && before[0].req.id == 1);

        auto res = e.handleNext();
        CHECK(res.ok() && res.value == 10);
        auto log = e.deliveries();
        CHECK(log.size() == 1);
        if (log.size() == 1) {
            CHECK(log[0].requestId == 2 && log[0].resource == ResourceType::Water);
        }
        CHECK(e.stock().water == 90 && e.stock().food == 0);

        // The food request is still waiting, untouched
        auto ranked = e.rankedRequests();
        CHECK(ranked.size() == 1 && ranked[0].req.id == 1 && ranked[0].req.quantity == 10);

        // Only the out-of-stock request is left: OutOfStock, and nothing changes
        res = e.handleNext();
        CHECK(res.err == Err::OutOfStock);
        CHECK(res.message == "Out of stock for food");
        CHECK(e.deliveries().size() == 1);
        CHECK(e.rankedRequests().size() == 1);

        // Restocking lets it through
        e.setStock(50, 90, 100);
        res = e.handleNext();
        CHECK(res.ok() && res.value == 10);
        CHECK(e.rankedRequests().empty());
    }
    {
        // 18. Unreachable and out-of-stock requests are both skipped, and the message says why
        Engine e;
        e.setAverageSpeed(60.0);
        setupMiniWorld(e);
        Camp c3 = {3, "Camp3", 20.0, 20.0, 100, 0, 0, 0};
        e.addCamp(c3, {});                                       // no road: unreachable
        e.setStock(100, 0, 100);
        e.submitRequest(3, ResourceType::Food, 10, 4, 10);       // id 1: rank 1, camp unreachable
        e.submitRequest(1, ResourceType::Water, 10, 3, 10);      // id 2: rank 2, water is 0
        e.submitRequest(2, ResourceType::Medicine, 10, 1, 10);   // id 3: rank 3, can be served
        e.processIncoming();

        auto res = e.handleNext();
        CHECK(res.ok());
        auto log = e.deliveries();
        CHECK(log.size() == 1 && log[0].requestId == 3);

        // Left: one unreachable, one out of stock. The stock-out is what gets reported.
        res = e.handleNext();
        CHECK(res.err == Err::OutOfStock);
        CHECK(res.message == "Out of stock for water");
        CHECK(e.rankedRequests().size() == 2);

        // Restock water: that request is served, and only the unreachable one remains
        e.setStock(100, 50, 100);
        CHECK(e.handleNext().ok());
        res = e.handleNext();
        CHECK(res.err == Err::Unreachable);
        CHECK(res.message.find("Unreachable: alternate delivery needed") == 0);
        CHECK(e.rankedRequests().size() == 1);
    }
    {
        // 18. BUG-2: coordinates must stay within +/-1,000,000 km, so a road length can never overflow
        Engine e;
        CHECK(e.addCamp({1, "far", 1e200, 0.0, 10, 0, 0, 0}, {0}).err == Err::Invalid);
        CHECK(e.addCamp({1, "far", 1000001.0, 0.0, 10, 0, 0, 0}, {0}).err == Err::Invalid);
        CHECK(e.campCount() == 0 && e.roads().empty());          // nothing left behind

        // exactly on the limit is fine, and the number of roads reported is the real number
        OpResult r = e.addCamp({1, "edge", 1000000.0, 0.0, 10, 0, 0, 0}, {0});
        CHECK(r.ok() && r.value == 1 && e.roads().size() == 1);

        // the longest possible road (corner to corner, about 2.83 million km) is accepted;
        // a genuine duplicate is still reported as a duplicate
        Engine e2;
        e2.addCamp({1, "A", 1000000.0, 1000000.0, 1, 0, 0, 0}, {});
        e2.addCamp({2, "B", -1000000.0, -1000000.0, 1, 0, 0, 0}, {});
        CHECK(e2.addRoad(1, 2).ok());
        CHECK(e2.addRoad(1, 2).err == Err::Duplicate);

        // two different points so close that their squared distance underflows to 0: refused, camp not added
        Engine e3;
        CHECK(e3.addCamp({1, "tiny", 1e-200, 0.0, 10, 0, 0, 0}, {0}).err == Err::Invalid);
        CHECK(e3.campCount() == 0 && e3.roads().empty());
    }
    {
        // 19. BUG-2: editCamp is all-or-nothing, so the camp never moves while its roads keep stale lengths
        Engine e;
        setupMiniWorld(e);   // camp 1 at (3, 4), camp 2 at (6, 8); roads 0-1 and 1-2, both 5 km
        CHECK(e.editCamp(1, "Camp1", 1e200, 0.0, 100).err == Err::Invalid);
        CHECK(e.editCamp(1, "Camp1", 1e-200, 0.0, 100).err == Err::Invalid);   // road 0-1 would be length 0
        Camp c;
        CHECK(e.findCamp(1, c) && c.x == 3.0 && c.y == 4.0);
        auto roads = e.roads();
        CHECK(roads.size() == 2);
        for (const auto& rd : roads) CHECK(rd.distanceKm == 5.0);

        // a valid move updates both roads: camp 1 to (6, 0) is 6 km from the depot and 8 km from camp 2
        CHECK(e.editCamp(1, "Camp1", 6.0, 0.0, 100).ok());
        roads = e.roads();
        CHECK(roads.size() == 2);
        for (const auto& rd : roads) CHECK(rd.distanceKm == ((rd.a == 0 || rd.b == 0) ? 6.0 : 8.0));
    }
    {
        // 20. BUG-2: the depot cannot be moved out of range either
        Engine e;
        setupMiniWorld(e);
        CHECK(e.setDepotPosition(1e200, 0.0).err == Err::Invalid);
        CHECK(e.setDepotPosition(0.0, -1000001.0).err == Err::Invalid);
        CHECK(e.depotX() == 0.0 && e.depotY() == 0.0);
        for (const auto& rd : e.roads()) CHECK(rd.distanceKm == 5.0);
        CHECK(e.setDepotPosition(-1000000.0, 1000000.0).ok());
    }
    {
        // 21. BUG-6: a negative camp id is not "the depot"
        Engine e;
        setupMiniWorld(e);
        OpResult r = e.deleteCamp(-5, false);
        CHECK(r.err == Err::Invalid && r.message == "Invalid camp id");
        r = e.deleteCamp(-1, true);
        CHECK(r.err == Err::Invalid && r.message == "Invalid camp id");
        r = e.deleteCamp(0, false);
        CHECK(r.err == Err::Invalid && r.message == "Cannot delete depot");
        CHECK(e.campCount() == 2);
    }
    {
        // 22. BUG-4: no stale AVL rotation report after a reset
        Engine e;
        for (int id : {10, 20, 30}) e.addCamp({id, "n", (double)id, 0.0, 1, 0, 0, 0}, {0});
        CHECK(!e.lastRotations().empty());
        e.reset();
        CHECK(e.campCount() == 0);
        CHECK(e.lastRotations().empty());
    }
    {
        // 23. BUG-5: a camp's received total stops at INT_MAX instead of overflowing
        Engine e;
        setupMiniWorld(e);
        const int big = std::numeric_limits<int>::max();
        for (int i = 0; i < 2; ++i) {
            e.setStock(big, 0, 0);
            CHECK(e.submitRequest(1, ResourceType::Food, big, 4, 5).ok());
            CHECK(e.processIncoming().ok());
            CHECK(e.handleNext().ok());
        }
        Camp c;
        CHECK(e.findCamp(1, c) && c.receivedFood == big);
    }
}
