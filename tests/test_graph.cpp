#include "test_helpers.h"
#include "../engine/Graph.h"

void runGraphTests() {
    {
        // 1. New graph
        Graph g;
        CHECK(g.nodeCount() == 0);
        CHECK(g.edgeCount() == 0);
        CHECK(g.averageSpeed() == 40.0);
    }
    {
        // 2. addNode
        Graph g;
        CHECK(g.addNode(5));
        CHECK(g.addNode(2));
        CHECK(g.addNode(9));
        CHECK(g.addNode(0));
        std::vector<int> out;
        g.nodeIds(out);
        CHECK(out.size() == 4);
        if (out.size() == 4) {
            CHECK(out[0] == 0);
            CHECK(out[1] == 2);
            CHECK(out[2] == 5);
            CHECK(out[3] == 9);
        }
        CHECK(!g.addNode(2)); // duplicate
        CHECK(!g.addNode(-1)); // negative
        CHECK(g.hasNode(5));
        CHECK(!g.hasNode(10));
    }
    {
        // 3. addEdge
        Graph g;
        g.addNode(1);
        g.addNode(2);
        CHECK(g.addEdge(1, 2, 5.0));
        CHECK(g.edgeCount() == 1);
        CHECK(!g.addEdge(1, 1, 5.0)); // a == b
        CHECK(!g.addEdge(1, 3, 5.0)); // missing node
        CHECK(!g.addEdge(1, 2, 0.0)); // dist = 0
        CHECK(!g.addEdge(1, 2, -5.0)); // negative dist
        CHECK(!g.addEdge(1, 2, 3.0)); // duplicate (a,b)
        CHECK(!g.addEdge(2, 1, 3.0)); // duplicate (b,a)
        CHECK(g.edgeCount() == 1);
    }
    {
        // 4. Undirected
        Graph g;
        g.addNode(1);
        g.addNode(2);
        g.addEdge(1, 2, 7.0);
        EdgeInfo e1, e2;
        CHECK(g.getEdge(1, 2, e1));
        CHECK(g.getEdge(2, 1, e2));
        CHECK(e1.distanceKm == 7.0);
        CHECK(e1.status == RoadStatus::Open);
        CHECK(e1.a == 1 && e1.b == 2);
        CHECK(e2.distanceKm == 7.0);
        CHECK(e2.status == RoadStatus::Open);
        CHECK(e2.a == 1 && e2.b == 2);
    }
    {
        // 5. computeTime
        Graph g;
        g.setAverageSpeed(60.0);
        CHECK(g.computeTime(7.0, RoadStatus::Open) == 7);
        CHECK(g.computeTime(7.0, RoadStatus::Slow) == 14);
        CHECK(g.computeTime(7.0, RoadStatus::Blocked) == 7);
        CHECK(g.computeTime(0.4, RoadStatus::Open) == 1); // minimum
        
        g.setAverageSpeed(40.0);
        CHECK(g.computeTime(10.0, RoadStatus::Open) == 15);
        CHECK(g.computeTime(10.0, RoadStatus::Slow) == 30);
        CHECK(g.computeTime(5.0, RoadStatus::Open) == 8); // 7.5 rounds up
    }
    {
        // 6. setStatus
        Graph g;
        g.addNode(1);
        g.addNode(2);
        g.setAverageSpeed(60.0);
        g.addEdge(1, 2, 7.0);
        CHECK(g.setStatus(1, 2, RoadStatus::Slow));
        EdgeInfo e1, e2;
        g.getEdge(1, 2, e1);
        g.getEdge(2, 1, e2);
        CHECK(e1.status == RoadStatus::Slow);
        CHECK(e1.minutes == 14);
        CHECK(e2.status == RoadStatus::Slow);
        CHECK(e2.minutes == 14);
        
        CHECK(g.setStatus(1, 2, RoadStatus::Open));
        g.getEdge(1, 2, e1);
        CHECK(e1.status == RoadStatus::Open);
        CHECK(e1.minutes == 7);
        
        CHECK(!g.setStatus(1, 3, RoadStatus::Blocked)); // missing road
    }
    {
        // 7. getNeighbors / getEdgesOf
        Graph g;
        g.addNode(0);
        g.addNode(1);
        g.addNode(2);
        g.addNode(3);
        // Add in random order
        g.addEdge(0, 3, 5.0);
        g.addEdge(0, 1, 5.0);
        g.addEdge(0, 2, 5.0);
        g.setStatus(0, 2, RoadStatus::Blocked);
        
        std::vector<Neighbor> n;
        g.getNeighbors(0, n);
        CHECK(n.size() == 2);
        if (n.size() == 2) {
            CHECK(n[0].id == 1);
            CHECK(n[1].id == 3);
        }
        
        std::vector<EdgeInfo> edges;
        g.getEdgesOf(0, edges);
        CHECK(edges.size() == 3);
        if (edges.size() == 3) {
            CHECK(edges[0].b == 1);
            CHECK(edges[1].b == 2);
            CHECK(edges[2].b == 3);
            CHECK(edges[1].status == RoadStatus::Blocked);
        }
    }
    {
        // 8. setAverageSpeed
        Graph g;
        g.addNode(1);
        g.addNode(2);
        g.setAverageSpeed(60.0);
        g.addEdge(1, 2, 7.0);
        EdgeInfo e;
        g.getEdge(1, 2, e);
        CHECK(e.minutes == 7);
        
        CHECK(g.setAverageSpeed(30.0));
        g.getEdge(1, 2, e);
        CHECK(e.minutes == 14);
        
        CHECK(!g.setAverageSpeed(0.0));
        CHECK(!g.setAverageSpeed(-5.0));
        g.getEdge(1, 2, e);
        CHECK(e.minutes == 14); // unchanged
    }
    {
        // 9. setEdgeDistance
        Graph g;
        g.addNode(1);
        g.addNode(2);
        g.setAverageSpeed(60.0);
        g.addEdge(1, 2, 7.0);
        
        CHECK(g.setEdgeDistance(1, 2, 12.0));
        EdgeInfo e;
        g.getEdge(1, 2, e);
        CHECK(e.distanceKm == 12.0);
        CHECK(e.minutes == 12);
        
        CHECK(!g.setEdgeDistance(1, 3, 12.0)); // missing
        CHECK(!g.setEdgeDistance(1, 2, 0.0)); // dist <= 0
        CHECK(!g.setEdgeDistance(1, 2, -1.0));
    }
    {
        // 10. removeEdge
        Graph g;
        g.addNode(1);
        g.addNode(2);
        g.addEdge(1, 2, 7.0);
        CHECK(g.edgeCount() == 1);
        
        CHECK(g.removeEdge(1, 2));
        CHECK(g.edgeCount() == 0);
        EdgeInfo e;
        CHECK(!g.getEdge(1, 2, e));
        CHECK(!g.getEdge(2, 1, e));
        
        CHECK(!g.removeEdge(1, 2)); // second remove
    }
    {
        // 11. removeNode
        Graph g;
        g.addNode(0);
        g.addNode(1);
        g.addNode(2);
        g.addNode(3);
        g.addEdge(0, 1, 5.0);
        g.addEdge(0, 2, 5.0);
        g.addEdge(0, 3, 5.0);
        g.addEdge(1, 2, 5.0);
        CHECK(g.nodeCount() == 4);
        CHECK(g.edgeCount() == 4);
        
        CHECK(g.removeNode(1));
        CHECK(g.nodeCount() == 3);
        CHECK(g.edgeCount() == 2); // edges 0-1 and 1-2 removed
        
        EdgeInfo e;
        CHECK(!g.getEdge(0, 1, e));
        CHECK(!g.getEdge(1, 2, e));
        
        std::vector<EdgeInfo> edges;
        g.getEdgesOf(2, edges);
        CHECK(edges.size() == 1);
        if (edges.size() == 1) {
            CHECK(edges[0].b == 0);
        }
        
        CHECK(!g.removeNode(99)); // missing
    }
    {
        // 12. edgeList
        Graph g;
        g.addNode(1);
        g.addNode(2);
        g.addNode(3);
        g.addEdge(2, 3, 1.0);
        g.addEdge(1, 3, 1.0);
        g.addEdge(1, 2, 1.0);
        
        std::vector<EdgeInfo> out;
        g.edgeList(out);
        CHECK(out.size() == 3);
        if (out.size() == 3) {
            CHECK(out[0].a == 1 && out[0].b == 2);
            CHECK(out[1].a == 1 && out[1].b == 3);
            CHECK(out[2].a == 2 && out[2].b == 3);
        }
    }
    {
        // 13. clear
        Graph g;
        g.addNode(1);
        g.addNode(2);
        g.addEdge(1, 2, 5.0);
        g.clear();
        CHECK(g.nodeCount() == 0);
        CHECK(g.edgeCount() == 0);
        
        // reuse
        CHECK(g.addNode(1));
        CHECK(g.nodeCount() == 1);
    }
    {
        // 14. Stress
        Graph g;
        for (int i = 0; i < 300; i++) {
            g.addNode(i);
        }
        for (int i = 0; i < 300; i++) {
            g.addEdge(i, (i + 1) % 300, 1.0);
        }
        CHECK(g.edgeCount() == 300);
        for (int i = 0; i < 300; i++) {
            g.removeNode(i);
        }
        CHECK(g.nodeCount() == 0);
        CHECK(g.edgeCount() == 0);
    }
}
