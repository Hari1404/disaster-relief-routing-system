#include "test_helpers.h"
#include "../engine/Engine.h"

void runScenarioTests() {
    {
        // 1. Scenario 1
        Engine e;
        std::string txt = readTextFile("data/scenario1_normal.txt");
        auto res = e.loadScenario(txt);
        CHECK(res.ok());
        CHECK(res.value == 6);
        CHECK(e.campCount() == 6);
        CHECK(e.roads().size() == 10);
        
        auto routes = e.routes();
        CHECK(routes.size() == 6);
        if (routes.size() == 6) {
            CHECK(routes[0].campId == 1 && routes[0].minutes == 5 && routes[0].path.size() == 2 && routes[0].path[1] == 1);
            CHECK(routes[1].campId == 2 && routes[1].minutes == 6 && routes[1].path.size() == 2 && routes[1].path[1] == 2);
            CHECK(routes[2].campId == 3 && routes[2].minutes == 10 && routes[2].path.size() == 3 && routes[2].path[2] == 3);
            CHECK(routes[3].campId == 4 && routes[3].minutes == 11 && routes[3].path.size() == 3 && routes[3].path[2] == 4);
            CHECK(routes[4].campId == 5 && routes[4].minutes == 12 && routes[4].path.size() == 3 && routes[4].path[2] == 5);
            CHECK(routes[5].campId == 6 && routes[5].minutes == 16 && routes[5].path.size() == 4 && routes[5].path[3] == 6);
        }
        
        auto steps = e.dijkstra().steps();
        CHECK(steps.size() >= 7);
        if (steps.size() >= 7) {
            CHECK(steps[0].node == 0 && steps[1].node == 1 && steps[2].node == 2);
            CHECK(steps[3].node == 3 && steps[4].node == 4 && steps[5].node == 5 && steps[6].node == 6);
        }
        
        auto q = e.queueContents();
        CHECK(q.size() == 6);
        if (q.size() == 6) {
            CHECK(q[0].id == 1 && q[1].id == 2 && q[2].id == 3 && q[3].id == 4 && q[4].id == 5 && q[5].id == 6);
        }
        
        e.processIncoming();
        auto ranked = e.rankedRequests();
        CHECK(ranked.size() == 6);
        if (ranked.size() == 6) {
            CHECK(ranked[0].req.id == 3);
            CHECK(ranked[1].req.id == 6);
            CHECK(ranked[2].req.id == 2);
            CHECK(ranked[3].req.id == 4);
            CHECK(ranked[4].req.id == 1);
            CHECK(ranked[5].req.id == 5);
            
            CHECK(ranked[0].reason == RankKey::Arrival);
            CHECK(ranked[1].reason == RankKey::Families);
            CHECK(ranked[2].reason == RankKey::Urgency);
            CHECK(ranked[3].reason == RankKey::Quantity);
            CHECK(ranked[4].reason == RankKey::Urgency);
        }
        
        res = e.handleNext(); CHECK(res.ok() && res.value == 20); // req 3
        res = e.handleNext(); CHECK(res.ok() && res.value == 20); // req 6
        res = e.handleNext(); CHECK(res.ok() && res.value == 200); // req 2
        res = e.handleNext(); CHECK(res.ok() && res.value == 80); // req 4
        res = e.handleNext(); CHECK(res.ok() && res.value == 50); // req 1
        res = e.handleNext(); CHECK(res.ok() && res.value == 500); // req 5
        
        CHECK(e.handleNext().err == Err::Empty);
        
        auto stock = e.stock();
        CHECK(stock.food == 870 && stock.water == 300 && stock.medicine == 960);
        CHECK(e.deliveries().size() == 6);
        
        Camp c;
        e.findCamp(1, c); CHECK(c.receivedFood == 130);
        e.findCamp(2, c); CHECK(c.receivedWater == 200 && c.receivedMedicine == 20);
        e.findCamp(3, c); CHECK(c.receivedMedicine == 20);
        e.findCamp(4, c); CHECK(c.receivedWater == 500);
    }
    {
        // 2. Scenario 2
        Engine e;
        std::string txt = readTextFile("data/scenario2_blockage.txt");
        auto res = e.loadScenario(txt);
        CHECK(res.ok() && res.value == 1);
        
        e.setRoadStatus(3, 6, RoadStatus::Blocked);
        auto routes = e.routes();
        if (routes.size() == 6) {
            CHECK(routes[5].campId == 6 && routes[5].minutes == 20);
            CHECK(routes[4].minutes == 12);
        }
        
        e.setRoadStatus(5, 6, RoadStatus::Blocked);
        routes = e.routes();
        if (routes.size() == 6) {
            CHECK(!routes[5].reachable);
            CHECK(routes[4].minutes == 12);
        }
        
        e.processIncoming();
        res = e.handleNext();
        CHECK(res.err == Err::Unreachable);
        CHECK(res.message.find("Unreachable: alternate delivery needed") == 0);
        CHECK(e.rankedRequests().size() == 1);
        CHECK(e.deliveries().empty());
        
        e.setRoadStatus(5, 6, RoadStatus::Open);
        res = e.handleNext();
        CHECK(res.ok() && res.value == 40);
        CHECK(e.stock().food == 960);
    }
    {
        // 3. Scenario 3
        Engine e;
        std::string txt = readTextFile("data/scenario3_priority.txt");
        auto res = e.loadScenario(txt);
        CHECK(res.ok() && res.value == 3);
        
        e.processIncoming();
        auto ranked = e.rankedRequests();
        CHECK(ranked.size() == 3);
        if (ranked.size() == 3) {
            CHECK(ranked[0].req.id == 2 && ranked[1].req.id == 3 && ranked[2].req.id == 1);
            CHECK(ranked[0].reason == RankKey::Urgency);
            CHECK(ranked[1].reason == RankKey::Urgency);
        }
        
        res = e.handleNext(); CHECK(res.ok() && res.value == 30); CHECK(e.stock().food == 70);
        res = e.handleNext(); CHECK(res.ok() && res.value == 40); CHECK(e.stock().water == 60);
        res = e.handleNext(); CHECK(res.ok() && res.value == 70); CHECK(e.stock().food == 0);
        
        ranked = e.rankedRequests();
        CHECK(ranked.size() == 1 && ranked[0].req.quantity == 130);
        
        res = e.handleNext();
        CHECK(res.err == Err::OutOfStock);
        CHECK(e.rankedRequests().size() == 1 && e.rankedRequests()[0].req.quantity == 130);
    }
    {
        // 4. Scenario 4
        Engine e;
        std::string txt = readTextFile("data/scenario4_avl.txt");
        auto res = e.loadScenario(txt);
        CHECK(res.ok() && res.value == 0);
        CHECK(e.campCount() == 6);
        CHECK(e.roads().size() == 6);
        
        std::vector<AVLNodeInfo> out;
        e.avlSnapshot(out);
        CHECK(out.size() == 6);
        if (out.size() == 6) {
            CHECK(out[0].id == 30 && out[0].height == 3 && out[0].balance == 0);
            CHECK(out[1].id == 20 && out[1].height == 2 && out[1].balance == 0);
            CHECK(out[2].id == 10 && out[2].height == 1 && out[2].balance == 0);
            CHECK(out[3].id == 25 && out[3].height == 1 && out[3].balance == 0);
            CHECK(out[4].id == 40 && out[4].height == 2 && out[4].balance == -1);
            CHECK(out[5].id == 50 && out[5].height == 1 && out[5].balance == 0);
        }
        
        auto routes = e.routes();
        CHECK(routes.size() == 6);
        if (routes.size() == 6) {
            CHECK(routes[0].campId == 10 && routes[0].minutes == 5);
            CHECK(routes[1].campId == 20 && routes[1].minutes == 10);
            CHECK(routes[2].campId == 25 && routes[2].minutes == 10);
            CHECK(routes[3].campId == 30 && routes[3].minutes == 15);
            CHECK(routes[4].campId == 40 && routes[4].minutes == 20);
            CHECK(routes[5].campId == 50 && routes[5].minutes == 25);
        }
        
        std::string trace = e.lastTrace();
        CHECK(trace.find("AVL rotations: RR at node 10, RR at node 30, RL at node 20") != std::string::npos);
    }
    {
        // 5. A load replaces everything
        Engine e;
        e.loadScenario(readTextFile("data/scenario1_normal.txt"));
        CHECK(e.campCount() == 6);
        
        e.loadScenario(readTextFile("data/scenario4_avl.txt"));
        CHECK(e.campCount() == 6); // happens to be 6 again, but different ids
        Camp c;
        CHECK(!e.findCamp(1, c));
        CHECK(!e.findCamp(6, c));
        CHECK(e.findCamp(10, c));
        CHECK(e.queueContents().empty());
    }
    {
        // 6. Bad scenario text
        Engine e;
        CHECK(e.loadScenario("xyz 1 2\n").err == Err::Invalid);
        CHECK(e.loadScenario("xyz 1 2\n").message.find("line 1:") == 0);
        
        CHECK(e.loadScenario("camp 1 A 1 1 1\nroad 0 7\n").err == Err::Invalid); // 7 doesn't exist
        
        CHECK(e.loadScenario("camp 1 A 1 1 1\ncamp 1 B 2 2 2\n").err == Err::Invalid); // duplicate
        
        CHECK(e.loadScenario("camp 1 A abc 4 5\n").err == Err::Invalid); // bad number
        
        CHECK(e.loadScenario("camp 1 A 3\n").err == Err::Invalid); // too few words
        
        CHECK(e.loadScenario("request 9 food 10 2 5\n").err == Err::Invalid); // unknown camp
        
        CHECK(e.loadScenario("camp 1 A 1 1 1\nrequest 1 rice 10 2 5\n").err == Err::Invalid); // unknown res

        // "nan" and "inf" parse as numbers in C, but are not valid map values
        CHECK(e.loadScenario("camp 1 A nan 4 5\n").err == Err::Invalid);
        CHECK(e.loadScenario("camp 1 A nan 4 5\n").message.find("line 1:") == 0);
        CHECK(e.loadScenario("camp 1 A 3 inf 5\n").err == Err::Invalid);
        CHECK(e.loadScenario("depot inf 0\n").err == Err::Invalid);
        CHECK(e.loadScenario("speed nan\n").err == Err::Invalid);
        CHECK(e.loadScenario("camp 99999999999 A 1 1 1\n").err == Err::Invalid); // too big for an int
        CHECK(e.loadScenario("camp 2147483648 A 1 1 1\n").err == Err::Invalid);   // INT_MAX + 1
        CHECK(e.loadScenario("camp -2147483649 A 1 1 1\n").err == Err::Invalid);  // INT_MIN - 1
        CHECK(e.loadScenario("camp 99999999999999999999 A 1 1 1\n").err == Err::Invalid); // beyond even 64 bits
        
        // ensure valid ignores blanks
        CHECK(e.loadScenario("\n# comment\ndepot 0 0\n").ok());
    }
    {
        // 7. Expected files exist
        CHECK(!readTextFile("data/scenario1_normal.expected.txt").empty());
        CHECK(!readTextFile("data/scenario2_blockage.expected.txt").empty());
        CHECK(!readTextFile("data/scenario3_priority.expected.txt").empty());
        CHECK(!readTextFile("data/scenario4_avl.expected.txt").empty());
    }
}
