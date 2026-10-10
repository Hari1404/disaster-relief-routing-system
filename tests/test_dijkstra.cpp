#include "test_helpers.h"
#include "../engine/Graph.h"
#include "../engine/Dijkstra.h"
#include <random>
#include <algorithm>

void runDijkstraTests() {
    {
        // Classic graph
        Graph g;
        g.setAverageSpeed(60.0);
        for (int i = 0; i <= 5; ++i) g.addNode(i);
        g.addEdge(0, 1, 7.0);
        g.addEdge(0, 2, 9.0);
        g.addEdge(0, 5, 14.0);
        g.addEdge(1, 2, 10.0);
        g.addEdge(1, 3, 15.0);
        g.addEdge(2, 3, 11.0);
        g.addEdge(2, 5, 2.0);
        g.addEdge(3, 4, 6.0);
        g.addEdge(4, 5, 9.0);
        
        Dijkstra d;
        d.run(g, 0);
        
        // 1. Expected paths and times
        std::vector<int> path;
        
        CHECK(d.getDistance(0) == 0);
        CHECK(d.reconstructPath(0, path) && path.size() == 1 && path[0] == 0);
        
        CHECK(d.getDistance(1) == 7);
        CHECK(d.reconstructPath(1, path) && path.size() == 2 && path[0] == 0 && path[1] == 1);
        
        CHECK(d.getDistance(2) == 9);
        CHECK(d.reconstructPath(2, path) && path.size() == 2 && path[0] == 0 && path[1] == 2);
        
        CHECK(d.getDistance(3) == 20);
        CHECK(d.reconstructPath(3, path) && path.size() == 3 && path[0] == 0 && path[1] == 2 && path[2] == 3);
        
        CHECK(d.getDistance(4) == 20);
        CHECK(d.reconstructPath(4, path) && path.size() == 4 && path[0] == 0 && path[1] == 2 && path[2] == 5 && path[3] == 4);
        
        CHECK(d.getDistance(5) == 11);
        CHECK(d.reconstructPath(5, path) && path.size() == 3 && path[0] == 0 && path[1] == 2 && path[2] == 5);
        
        // 2. Steps
        const auto& steps = d.steps();
        CHECK(steps.size() == 6);
        if (steps.size() == 6) {
            CHECK(steps[0].node == 0 && steps[0].minutes == 0);
            CHECK(steps[0].improved.size() == 3 && steps[0].improved[0] == 1 && steps[0].improved[1] == 2 && steps[0].improved[2] == 5);
            
            CHECK(steps[1].node == 1 && steps[1].minutes == 7);
            CHECK(steps[1].improved.size() == 1 && steps[1].improved[0] == 3);
            
            CHECK(steps[2].node == 2 && steps[2].minutes == 9);
            CHECK(steps[2].improved.size() == 2 && steps[2].improved[0] == 3 && steps[2].improved[1] == 5);
            
            CHECK(steps[3].node == 5 && steps[3].minutes == 11);
            CHECK(steps[3].improved.size() == 1 && steps[3].improved[0] == 4);
            
            CHECK(steps[4].node == 3 && steps[4].minutes == 20);
            CHECK(steps[4].improved.size() == 0);
            
            CHECK(steps[5].node == 4 && steps[5].minutes == 20);
            CHECK(steps[5].improved.size() == 0);
        }
        
        // 3. Blocked road
        g.setStatus(2, 5, RoadStatus::Blocked);
        d.run(g, 0);
        CHECK(d.getDistance(1) == 7);
        CHECK(d.getDistance(2) == 9);
        CHECK(d.getDistance(3) == 20);
        CHECK(d.reconstructPath(3, path) && path.size() == 3 && path[0] == 0 && path[1] == 2 && path[2] == 3);
        CHECK(d.getDistance(4) == 23);
        CHECK(d.reconstructPath(4, path) && path.size() == 3 && path[0] == 0 && path[1] == 5 && path[2] == 4);
        CHECK(d.getDistance(5) == 14);
        CHECK(d.reconstructPath(5, path) && path.size() == 2 && path[0] == 0 && path[1] == 5);
        g.setStatus(2, 5, RoadStatus::Open);
        
        // 4. Slow road
        g.setStatus(0, 1, RoadStatus::Slow);
        d.run(g, 0);
        CHECK(d.getDistance(1) == 14);
        CHECK(d.reconstructPath(1, path) && path.size() == 2 && path[0] == 0 && path[1] == 1);
        CHECK(d.getDistance(2) == 9);
        CHECK(d.getDistance(3) == 20);
        CHECK(d.getDistance(4) == 20);
        CHECK(d.getDistance(5) == 11);
        g.setStatus(0, 1, RoadStatus::Open);
        
        // 5. Unreachable camp
        g.setStatus(2, 3, RoadStatus::Blocked);
        g.setStatus(1, 3, RoadStatus::Blocked);
        g.setStatus(3, 4, RoadStatus::Blocked);
        d.run(g, 0);
        CHECK(!d.isReachable(3));
        CHECK(d.getDistance(3) == UNREACHABLE);
        std::vector<int> p;
        p.push_back(99);
        CHECK(!d.reconstructPath(3, p));
        CHECK(p.empty());
        CHECK(d.getDistance(1) == 7);
        CHECK(d.getDistance(2) == 9);
        CHECK(d.getDistance(4) == 20);
        CHECK(d.getDistance(5) == 11);
    }
    {
        // 6. Equal-weight tie
        Graph g;
        g.setAverageSpeed(60.0);
        for (int i = 0; i <= 3; ++i) g.addNode(i);
        g.addEdge(0, 1, 2.0);
        g.addEdge(0, 2, 2.0);
        g.addEdge(1, 3, 3.0);
        g.addEdge(2, 3, 3.0);
        Dijkstra d;
        d.run(g, 0);
        CHECK(d.getDistance(3) == 5);
        std::vector<int> path;
        CHECK(d.reconstructPath(3, path) && path.size() == 3);
        if (path.size() == 3) {
            CHECK(path[0] == 0 && path[1] == 1 && path[2] == 3);
        }
    }
    {
        // 7. Edge cases
        Graph g;
        g.setAverageSpeed(60.0);
        g.addNode(0);
        Dijkstra d;
        d.run(g, 0);
        CHECK(d.getDistance(0) == 0);
        std::vector<int> path;
        CHECK(d.reconstructPath(0, path) && path.size() == 1 && path[0] == 0);
        CHECK(d.getDistance(99) == UNREACHABLE);
        CHECK(!d.isReachable(99));
        
        d.run(g, 99);
        CHECK(d.getDistance(0) == UNREACHABLE);
        CHECK(d.steps().empty());
        
        g.addNode(1);
        d.run(g, 0);
        CHECK(d.getDistance(1) == UNREACHABLE);
    }
    {
        // 8. Re-running
        Graph g;
        g.setAverageSpeed(60.0);
        g.addNode(0);
        g.addNode(1);
        g.addEdge(0, 1, 5.0);
        Dijkstra d;
        d.run(g, 0);
        CHECK(d.getDistance(1) == 5);
        
        g.addNode(2);
        g.addEdge(1, 2, 3.0);
        d.run(g, 0);
        CHECK(d.getDistance(2) == 8);
    }
    {
        // 9. Randomised oracle
        int seeds[] = {1, 2, 3, 4, 42};
        for (int s : seeds) {
            std::mt19937 gen(s);
            std::uniform_real_distribution<> prob(0.0, 1.0);
            std::uniform_int_distribution<> length(1, 20);
            std::uniform_int_distribution<> stat(0, 9);
            
            Graph g;
            g.setAverageSpeed(60.0);
            std::vector<int> order;
            for (int i = 0; i < 25; ++i) order.push_back(i);
            std::shuffle(order.begin(), order.end(), gen);
            for (int id : order) g.addNode(id);
            
            for (int i = 0; i < 25; ++i) {
                for (int j = i + 1; j < 25; ++j) {
                    if (prob(gen) < 0.15) {
                        double len = length(gen);
                        g.addEdge(i, j, len);
                        int st = stat(gen);
                        if (st < 2) g.setStatus(i, j, RoadStatus::Blocked);
                        else if (st < 4) g.setStatus(i, j, RoadStatus::Slow);
                    }
                }
            }
            
            Dijkstra d;
            d.run(g, 0);
            
            std::vector<int> bf(25, UNREACHABLE);
            bf[0] = 0;
            
            std::vector<EdgeInfo> edges;
            g.edgeList(edges);
            
            for (int i = 0; i < 24; ++i) {
                for (const auto& e : edges) {
                    if (e.status == RoadStatus::Blocked) continue;
                    int mins = g.computeTime(e.distanceKm, e.status);
                    
                    if (bf[e.a] != UNREACHABLE) {
                        if (bf[e.b] == UNREACHABLE || bf[e.a] + mins < bf[e.b]) {
                            bf[e.b] = bf[e.a] + mins;
                        }
                    }
                    if (bf[e.b] != UNREACHABLE) {
                        if (bf[e.a] == UNREACHABLE || bf[e.b] + mins < bf[e.a]) {
                            bf[e.a] = bf[e.b] + mins;
                        }
                    }
                }
            }
            
            for (int i = 0; i < 25; ++i) {
                CHECK(d.getDistance(i) == bf[i]);
                if (bf[i] != UNREACHABLE) {
                    std::vector<int> path;
                    CHECK(d.reconstructPath(i, path));
                    CHECK(!path.empty());
                    CHECK(path.front() == 0);
                    CHECK(path.back() == i);
                    
                    int sum = 0;
                    for (size_t p = 0; p < path.size() - 1; ++p) {
                        EdgeInfo e;
                        CHECK(g.getEdge(path[p], path[p+1], e));
                        CHECK(e.status != RoadStatus::Blocked);
                        sum += e.minutes;
                    }
                    CHECK(sum == bf[i]);
                }
            }
        }
    }
}
