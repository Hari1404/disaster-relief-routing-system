#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include <vector>
#include "Graph.h"

extern const int UNREACHABLE;

struct DijkstraStep {
    int              node;       // the node picked in this step (smallest tentative time among unvisited reachable nodes)
    int              minutes;    // its final time
    std::vector<int> improved;   // ids of neighbours whose time got smaller in this step, in ascending id order
};

class Dijkstra {
public:
    // Clears any earlier result, then computes times and predecessors from source. O(V squared).
    // Note: the heap version would be O((V+E) log V).
    // If source is not a node of g, every node is UNREACHABLE and steps() is empty.
    void run(const Graph& g, int source);
    
    // Returns minutes to node, or UNREACHABLE (also for unknown ids). O(log V)
    int  getDistance(int node) const;
    
    // Returns true if reachable. false for UNREACHABLE and for unknown ids. O(log V)
    bool isReachable(int node) const;
    
    // On success: path = node ids from the source to node. Returns true.
    // If node is unreachable or unknown: path is cleared and it returns false. O(V)
    bool reconstructPath(int node, std::vector<int>& path) const;
    
    // Returns the steps recorded, one entry per picked node, in order. O(1)
    const std::vector<DijkstraStep>& steps() const;

private:
    // Returns index of the unvisited node with smallest time; -1 if none reachable. O(V)
    int  pickMinimum() const;
    
    // Updates the neighbours of node at uIndex. O(degree)
    void relax(const Graph& g, int uIndex, DijkstraStep& step);
    
    // Binary search in ids_; -1 if absent. O(log V)
    // The whole run is still O(V squared) because the O(log V) search happens
    // for each edge relaxed, adding only O(E log V) overall, which is dominated by O(V^2).
    int  indexOf(int id) const;

    std::vector<int> ids_;      // node ids, ascending (copied from g.nodeIds)
    std::vector<int> dist_;     // dist_[i] is the time to ids_[i], or UNREACHABLE
    std::vector<int> prev_;     // prev_[i] is the INDEX of the predecessor, or -1
    std::vector<int> visited_;  // 0 or 1
    std::vector<DijkstraStep> steps_;
};

#endif
