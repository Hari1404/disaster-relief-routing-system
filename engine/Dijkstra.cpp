#include "Dijkstra.h"

const int UNREACHABLE = -1;

// Binary search in ids_; -1 if absent. O(log V)
// The whole run is still O(V squared) because the binary search adds only O(E log V)
// to the total time, while the pickMinimum loop takes O(V^2). O(V^2 + E log V) = O(V^2).
int Dijkstra::indexOf(int id) const {
    int left = 0;
    int right = static_cast<int>(ids_.size()) - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (ids_[mid] == id) {
            return mid;
        } else if (ids_[mid] < id) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

void Dijkstra::run(const Graph& g, int source) {
    g.nodeIds(ids_);
    int n = static_cast<int>(ids_.size());
    
    dist_.assign(n, UNREACHABLE);
    prev_.assign(n, -1);
    visited_.assign(n, 0);
    steps_.clear();
    
    int sourceIndex = indexOf(source);
    if (sourceIndex == -1) {
        // Source not in graph
        return;
    }
    
    dist_[sourceIndex] = 0;
    
    while (true) {
        int uIndex = pickMinimum();
        if (uIndex == -1) {
            break;
        }
        
        visited_[uIndex] = 1;
        
        DijkstraStep step;
        step.node = ids_[uIndex];
        step.minutes = dist_[uIndex];
        
        relax(g, uIndex, step);
        steps_.push_back(step);
    }
}

int Dijkstra::pickMinimum() const {
    int minIndex = -1;
    int minDist = -1;
    
    for (size_t i = 0; i < dist_.size(); ++i) {
        if (!visited_[i] && dist_[i] != UNREACHABLE) {
            // Tie rule: On equal times the node with the smaller index (which is the smaller id) is picked.
            // Since we iterate in ascending order, the first one found is kept strictly (<).
            if (minDist == -1 || dist_[i] < minDist) {
                minDist = dist_[i];
                minIndex = static_cast<int>(i);
            }
        }
    }
    
    return minIndex;
}

void Dijkstra::relax(const Graph& g, int uIndex, DijkstraStep& step) {
    std::vector<Neighbor> neighbors;
    g.getNeighbors(ids_[uIndex], neighbors);
    
    int currentDist = dist_[uIndex];
    // Safe unreachable check: pickMinimum guarantees currentDist != UNREACHABLE.
    
    for (const auto& n : neighbors) {
        int vIndex = indexOf(n.id);
        if (vIndex != -1 && !visited_[vIndex]) {
            int newDist = currentDist + n.minutes;
            
            // Tie rule: updates a neighbour only if the new total is strictly smaller (<),
            // so on equal totals the path found first is kept.
            if (dist_[vIndex] == UNREACHABLE || newDist < dist_[vIndex]) {
                dist_[vIndex] = newDist;
                prev_[vIndex] = uIndex;
                step.improved.push_back(n.id);
            }
        }
    }
}

int Dijkstra::getDistance(int node) const {
    int idx = indexOf(node);
    if (idx == -1) return UNREACHABLE;
    return dist_[idx];
}

bool Dijkstra::isReachable(int node) const {
    return getDistance(node) != UNREACHABLE;
}

bool Dijkstra::reconstructPath(int node, std::vector<int>& path) const {
    path.clear();
    int idx = indexOf(node);
    if (idx == -1 || dist_[idx] == UNREACHABLE) {
        return false;
    }
    
    // Reconstruct backwards using indices
    std::vector<int> reverseIndices;
    int current = idx;
    while (current != -1) {
        reverseIndices.push_back(current);
        current = prev_[current];
    }
    
    // Reverse it for the final path
    for (int i = static_cast<int>(reverseIndices.size()) - 1; i >= 0; --i) {
        path.push_back(ids_[reverseIndices[i]]);
    }
    
    return true;
}

const std::vector<DijkstraStep>& Dijkstra::steps() const {
    return steps_;
}
