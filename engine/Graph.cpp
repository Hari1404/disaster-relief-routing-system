#include "Graph.h"
#include <cmath>

Graph::Graph() : head_(nullptr), nodeCount_(0), edgeCount_(0), avgSpeed_(40.0) {}

Graph::~Graph() {
    clear();
}

Graph::VertexNode* Graph::findVertex(int id) const {
    VertexNode* curr = head_;
    // Walk list to find id
    while (curr && curr->id <= id) {
        if (curr->id == id) return curr;
        curr = curr->next;
    }
    return nullptr;
}

bool Graph::addNode(int id) {
    if (id < 0) return false;

    VertexNode* prev = nullptr;
    VertexNode* curr = head_;

    // Find insertion point to maintain ascending order
    while (curr && curr->id < id) {
        prev = curr;
        curr = curr->next;
    }

    // Check for duplicate
    if (curr && curr->id == id) {
        return false;
    }

    // Allocate and insert new vertex
    VertexNode* v = new VertexNode{id, nullptr, curr};
    if (prev) {
        prev->next = v;
    } else {
        head_ = v;
    }
    nodeCount_++;
    return true;
}

bool Graph::removeNode(int id) {
    VertexNode* prevV = nullptr;
    VertexNode* currV = head_;

    // Find the vertex to remove
    while (currV && currV->id < id) {
        prevV = currV;
        currV = currV->next;
    }

    // Return false if not found
    if (!currV || currV->id != id) {
        return false;
    }

    // Step 1: Remove edges pointing to this vertex from neighbors
    // We walk through all edges of the current vertex
    EdgeNode* e = currV->edges;
    while (e) {
        VertexNode* neighbor = findVertex(e->to);
        if (neighbor) {
            removeEdgeFromVertex(neighbor, id);
        }
        edgeCount_--; // Decrement edge count for each undirected edge removed
        e = e->next;
    }

    // Step 2: Free the vertex's own edge list
    e = currV->edges;
    while (e) {
        EdgeNode* nextE = e->next;
        delete e;
        e = nextE;
    }

    // Step 3: Unlink and delete the vertex
    if (prevV) {
        prevV->next = currV->next;
    } else {
        head_ = currV->next;
    }
    delete currV;
    nodeCount_--;
    return true;
}

bool Graph::hasNode(int id) const {
    return findVertex(id) != nullptr;
}

// Two edge nodes per road: Helper to insert an edge into a vertex's list
void Graph::insertEdgeIntoVertex(VertexNode* v, int to, double distanceKm, RoadStatus status, int minutes) {
    EdgeNode* prev = nullptr;
    EdgeNode* curr = v->edges;
    
    // Find insertion point to maintain ascending 'to' order
    while (curr && curr->to < to) {
        prev = curr;
        curr = curr->next;
    }
    
    // Allocate and insert new edge
    EdgeNode* edge = new EdgeNode{to, distanceKm, status, minutes, curr};
    if (prev) {
        prev->next = edge;
    } else {
        v->edges = edge;
    }
}

// Helper to remove an edge from a vertex's list
bool Graph::removeEdgeFromVertex(VertexNode* v, int to) {
    EdgeNode* prev = nullptr;
    EdgeNode* curr = v->edges;
    
    // Find the edge
    while (curr && curr->to != to) {
        prev = curr;
        curr = curr->next;
    }
    
    // Return false if not found
    if (!curr) return false;
    
    // Unlink and delete
    if (prev) {
        prev->next = curr->next;
    } else {
        v->edges = curr->next;
    }
    delete curr;
    return true;
}

bool Graph::addEdge(int a, int b, double distanceKm) {
    if (a == b || distanceKm <= 0) return false;

    VertexNode* vA = findVertex(a);
    VertexNode* vB = findVertex(b);
    if (!vA || !vB) return false;

    // Check if road already exists
    EdgeNode* curr = vA->edges;
    while (curr && curr->to <= b) {
        if (curr->to == b) return false;
        curr = curr->next;
    }

    int mins = computeTime(distanceKm, RoadStatus::Open);
    
    // Insert into both endpoints (two edge nodes per road)
    insertEdgeIntoVertex(vA, b, distanceKm, RoadStatus::Open, mins);
    insertEdgeIntoVertex(vB, a, distanceKm, RoadStatus::Open, mins);
    
    edgeCount_++;
    return true;
}

bool Graph::removeEdge(int a, int b) {
    VertexNode* vA = findVertex(a);
    VertexNode* vB = findVertex(b);
    if (!vA || !vB) return false;

    // Remove from both endpoints
    bool removedA = removeEdgeFromVertex(vA, b);
    bool removedB = removeEdgeFromVertex(vB, a);
    
    if (!removedA || !removedB) return false;
    
    edgeCount_--;
    return true;
}

bool Graph::setEdgeDistance(int a, int b, double distanceKm) {
    if (distanceKm <= 0) return false;
    
    VertexNode* vA = findVertex(a);
    VertexNode* vB = findVertex(b);
    if (!vA || !vB) return false;

    EdgeNode* eA = vA->edges;
    while (eA && eA->to != b) eA = eA->next;
    
    EdgeNode* eB = vB->edges;
    while (eB && eB->to != a) eB = eB->next;
    
    if (!eA || !eB) return false;
    
    int mins = computeTime(distanceKm, eA->status);
    
    // Update both endpoints
    eA->distanceKm = distanceKm;
    eA->minutes = mins;
    
    eB->distanceKm = distanceKm;
    eB->minutes = mins;
    
    return true;
}

bool Graph::setStatus(int a, int b, RoadStatus s) {
    VertexNode* vA = findVertex(a);
    VertexNode* vB = findVertex(b);
    if (!vA || !vB) return false;

    EdgeNode* eA = vA->edges;
    while (eA && eA->to != b) eA = eA->next;
    
    EdgeNode* eB = vB->edges;
    while (eB && eB->to != a) eB = eB->next;
    
    if (!eA || !eB) return false;
    
    int mins = computeTime(eA->distanceKm, s);
    
    // Update both endpoints
    eA->status = s;
    eA->minutes = mins;
    
    eB->status = s;
    eB->minutes = mins;
    
    return true;
}

bool Graph::setAverageSpeed(double kmh) {
    if (kmh <= 0) return false;
    avgSpeed_ = kmh;
    
    VertexNode* v = head_;
    // Walk through all vertices and recompute times for their edges
    while (v) {
        EdgeNode* e = v->edges;
        while (e) {
            e->minutes = computeTime(e->distanceKm, e->status);
            e = e->next;
        }
        v = v->next;
    }
    return true;
}

double Graph::averageSpeed() const {
    return avgSpeed_;
}

int Graph::computeTime(double distanceKm, RoadStatus s) const {
    double speed = avgSpeed_;
    if (s == RoadStatus::Slow) {
        speed /= 2.0;
    }
    int minutes = std::lround(distanceKm / speed * 60.0);
    return minutes < 1 ? 1 : minutes;
}

bool Graph::getEdge(int a, int b, EdgeInfo& out) const {
    VertexNode* vA = findVertex(a);
    if (!vA) return false;
    
    EdgeNode* e = vA->edges;
    // Walk to find edge
    while (e && e->to != b) {
        e = e->next;
    }
    if (!e) return false;
    
    out.a = (a < b) ? a : b;
    out.b = (a > b) ? a : b;
    out.distanceKm = e->distanceKm;
    out.status = e->status;
    out.minutes = e->minutes;
    return true;
}

void Graph::getEdgesOf(int id, std::vector<EdgeInfo>& out) const {
    out.clear();
    VertexNode* v = findVertex(id);
    if (!v) return;
    
    EdgeNode* e = v->edges;
    // Edges are already stored in ascending 'to' order
    while (e) {
        EdgeInfo info;
        info.a = id;
        info.b = e->to;
        info.distanceKm = e->distanceKm;
        info.status = e->status;
        info.minutes = e->minutes;
        out.push_back(info);
        e = e->next;
    }
}

void Graph::getNeighbors(int id, std::vector<Neighbor>& out) const {
    out.clear();
    VertexNode* v = findVertex(id);
    if (!v) return;
    
    EdgeNode* e = v->edges;
    // Walk edges, skipping blocked
    while (e) {
        if (e->status != RoadStatus::Blocked) {
            out.push_back({e->to, e->minutes});
        }
        e = e->next;
    }
}

void Graph::edgeList(std::vector<EdgeInfo>& out) const {
    out.clear();
    VertexNode* v = head_;
    // Walk all vertices
    while (v) {
        EdgeNode* e = v->edges;
        // Walk all edges of vertex
        while (e) {
            // Only add each edge once, where a < b
            if (v->id < e->to) {
                out.push_back({v->id, e->to, e->distanceKm, e->status, e->minutes});
            }
            e = e->next;
        }
        v = v->next;
    }
}

void Graph::nodeIds(std::vector<int>& out) const {
    out.clear();
    VertexNode* v = head_;
    // Nodes are already in ascending order
    while (v) {
        out.push_back(v->id);
        v = v->next;
    }
}

int Graph::nodeCount() const {
    return nodeCount_;
}

int Graph::edgeCount() const {
    return edgeCount_;
}

void Graph::clear() {
    VertexNode* curr = head_;
    // Walk vertices to delete
    while (curr) {
        EdgeNode* e = curr->edges;
        // Walk edges to delete
        while (e) {
            EdgeNode* nextE = e->next;
            delete e;
            e = nextE;
        }
        VertexNode* nextV = curr->next;
        delete curr;
        curr = nextV;
    }
    head_ = nullptr;
    nodeCount_ = 0;
    edgeCount_ = 0;
}
