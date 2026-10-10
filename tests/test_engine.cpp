#include "test_helpers.h"
#include "../engine/Engine.h"

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
}
