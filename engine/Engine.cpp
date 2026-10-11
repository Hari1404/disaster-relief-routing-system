#include "Engine.h"
#include <cmath>

// Straight-line length between two points, in km.
static double lineLength(double x1, double y1, double x2, double y2) {
    return std::sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

// A road length the graph will accept: finite and greater than 0.
// (The negated comparison also rejects NaN.)
static bool validLength(double d) {
    return std::isfinite(d) && d > 0.0;
}

Engine::Engine() {
    reset();
}

Engine::~Engine() {
}

void Engine::reset() {
    camps_.clear();
    graph_.clear();
    graph_.addNode(0);
    queue_.clear();
    activeRequests_.clear();
    log_.clear();
    
    depotX_ = 0.0;
    depotY_ = 0.0;
    graph_.setAverageSpeed(40.0);
    stock_ = {500, 500, 500};
    
    nextRequestId_ = 1;
    nextArrivalSeq_ = 1;
    
    runDijkstra();
    lastTrace_ = "Reset.";
}

bool Engine::positionOf(int id, double& outX, double& outY) const {
    if (id == 0) {
        outX = depotX_;
        outY = depotY_;
        return true;
    }
    Camp c;
    if (camps_.search(id, c)) {
        outX = c.x;
        outY = c.y;
        return true;
    }
    return false;
}

double Engine::distanceBetween(int a, int b) const {
    double ax, ay, bx, by;
    positionOf(a, ax, ay);
    positionOf(b, bx, by);
    return lineLength(ax, ay, bx, by);
}

void Engine::runDijkstra() {
    dijkstra_.run(graph_, 0);
}

OpResult Engine::addCamp(const Camp& c, const std::vector<int>& linkTo) {
    if (!isValidCamp(c)) {
        return {Err::Invalid, "Invalid camp data", 0};
    }
    Camp cCopy = c;
    cCopy.receivedFood = 0;
    cCopy.receivedWater = 0;
    cCopy.receivedMedicine = 0;

    Camp tmp;
    if (camps_.search(cCopy.id, tmp)) {
        return {Err::Duplicate, "Camp id already exists", 0};
    }

    // Check links, and work out every road length now, BEFORE anything is changed,
    // so a bad length can never leave a half-added camp behind.
    std::vector<double> linkLength;
    for (int link : linkTo) {
        if (link == cCopy.id) return {Err::Invalid, "Link to itself", 0};
        if (link != 0 && !camps_.search(link, tmp)) {
            return {Err::NotFound, "Link target not found", 0};
        }
        double tx, ty;
        positionOf(link, tx, ty);
        if (cCopy.x == tx && cCopy.y == ty) {
            return {Err::Invalid, "Link distance would be 0", 0};
        }
        double len = lineLength(cCopy.x, cCopy.y, tx, ty);
        if (!validLength(len)) {
            return {Err::Invalid, "Link distance out of range", 0};
        }
        linkLength.push_back(len);
    }
    
    // Check duplicate links
    for (size_t i = 0; i < linkTo.size(); ++i) {
        for (size_t j = i + 1; j < linkTo.size(); ++j) {
            if (linkTo[i] == linkTo[j]) return {Err::Invalid, "Duplicate links", 0};
        }
    }

    camps_.insert(cCopy);
    graph_.addNode(cCopy.id);
    int roadsAdded = 0;
    
    for (size_t i = 0; i < linkTo.size(); ++i) {
        // Every length was checked above, so this only fails if the graph itself disagrees.
        // Count only the roads that really exist, so the reported number is true.
        if (graph_.addEdge(cCopy.id, linkTo[i], linkLength[i])) roadsAdded++;
    }
    
    runDijkstra();
    
    // Build trace
    const auto& rots = camps_.lastRotations();
    if (rots.empty()) {
        lastTrace_ = "AVL insert " + std::to_string(cCopy.id) + ": no rotation | Dijkstra rerun from depot";
    } else {
        std::string rotName;
        switch (rots[0].type) {
            case RotationType::LL: rotName = "LL"; break;
            case RotationType::LR: rotName = "LR"; break;
            case RotationType::RR: rotName = "RR"; break;
            case RotationType::RL: rotName = "RL"; break;
        }
        lastTrace_ = "AVL insert " + std::to_string(cCopy.id) + " caused " + rotName + " rotation at node " + std::to_string(rots[0].nodeId) + " | Dijkstra rerun from depot";
    }
    return {Err::None, "Camp added", roadsAdded};
}

OpResult Engine::editCamp(int id, const std::string& name, double x, double y, int families) {
    if (name.empty() || families < 0 || !isValidCoordinate(x) || !isValidCoordinate(y)) {
        return {Err::Invalid, "Invalid camp data", 0};
    }

    Camp c;
    if (!camps_.search(id, c)) return {Err::NotFound, "Camp not found", 0};
    
    // Work out the new length of every road at this camp BEFORE changing anything,
    // so a bad length can never leave the camp moved but its roads stale.
    std::vector<EdgeInfo> edges;
    graph_.getEdgesOf(id, edges);
    std::vector<double> newLength;
    for (const auto& e : edges) {
        double tx, ty;
        positionOf(e.b, tx, ty);          // node 0 gives the depot position
        if (tx == x && ty == y) {
            return {Err::Invalid, "Road length would become 0", 0};
        }
        double len = lineLength(x, y, tx, ty);
        if (!validLength(len)) {
            return {Err::Invalid, "Road length out of range", 0};
        }
        newLength.push_back(len);
    }
    
    camps_.update(id, name, x, y, families);
    
    for (size_t i = 0; i < edges.size(); ++i) {
        if (!graph_.setEdgeDistance(id, edges[i].b, newLength[i])) {
            return {Err::Invalid, "Could not update a road length", 0};
        }
    }
    
    runDijkstra();
    lastTrace_ = "AVL update " + std::to_string(id) + ": key unchanged, no rotation | Dijkstra rerun from depot";
    return {Err::None, "Camp updated", 0};
}

OpResult Engine::deleteCamp(int id, bool confirm) {
    Camp tmpC;
    if (id == 0) return {Err::Invalid, "Cannot delete depot", 0};
    if (id < 0) return {Err::Invalid, "Invalid camp id", 0};
    if (!camps_.search(id, tmpC)) return {Err::NotFound, "Camp not found", 0};
    
    int pending = pendingCountForCamp(id);
    if (pending > 0 && !confirm) {
        return {Err::ConfirmNeeded, "Pending requests will be cancelled", pending};
    }
    
    camps_.remove(id);
    queue_.removeByCamp(id);
    activeRequests_.deleteByCamp(id);
    graph_.removeNode(id);
    
    runDijkstra();
    
    std::string rotStr;
    const auto& rots = camps_.lastRotations();
    if (rots.empty()) {
        rotStr = "no rotation";
    } else {
        bool first = true;
        rotStr = "caused ";
        for (const auto& r : rots) {
            if (!first) rotStr += "; ";
            std::string rName;
            switch (r.type) {
                case RotationType::LL: rName = "LL"; break;
                case RotationType::LR: rName = "LR"; break;
                case RotationType::RR: rName = "RR"; break;
                case RotationType::RL: rName = "RL"; break;
            }
            rotStr += rName + " rotation at node " + std::to_string(r.nodeId);
            first = false;
        }
    }
    
    lastTrace_ = "AVL delete " + std::to_string(id) + ": " + rotStr + " | Dijkstra rerun from depot";
    return {Err::None, "Camp deleted", pending};
}

OpResult Engine::setDepotPosition(double x, double y) {
    if (!isValidCoordinate(x) || !isValidCoordinate(y)) {
        return {Err::Invalid, "Depot position must be a number within +/-1000000 km", 0};
    }
    // Work out every new road length first, before changing anything.
    std::vector<EdgeInfo> edges;
    graph_.getEdgesOf(0, edges);
    std::vector<double> newLength;
    for (const auto& e : edges) {
        double tx, ty;
        positionOf(e.b, tx, ty);
        if (tx == x && ty == y) {
            return {Err::Invalid, "Road length would become 0", 0};
        }
        double len = lineLength(x, y, tx, ty);
        if (!validLength(len)) {
            return {Err::Invalid, "Road length out of range", 0};
        }
        newLength.push_back(len);
    }
    
    depotX_ = x;
    depotY_ = y;
    
    for (size_t i = 0; i < edges.size(); ++i) {
        if (!graph_.setEdgeDistance(0, edges[i].b, newLength[i])) {
            return {Err::Invalid, "Could not update a road length", 0};
        }
    }
    
    runDijkstra();
    lastTrace_ = "Depot position set to " + std::to_string(x) + ", " + std::to_string(y) + " | Dijkstra rerun from depot";
    return {Err::None, "Depot position updated", 0};
}

OpResult Engine::addRoad(int a, int b) {
    Camp tmpC;
    if (a == b) return {Err::Invalid, "Same node", 0};
    if (a != 0 && !camps_.search(a, tmpC)) return {Err::NotFound, "Node a not found", 0};
    if (b != 0 && !camps_.search(b, tmpC)) return {Err::NotFound, "Node b not found", 0};
    
    double dist = distanceBetween(a, b);
    if (!validLength(dist)) {
        return {Err::Invalid, dist == 0.0 ? "Distance is 0" : "Distance out of range", 0};
    }
    
    // The nodes exist and the length is valid, so the only reason left for a refusal is a duplicate road.
    if (!graph_.addEdge(a, b, dist)) {
        return {Err::Duplicate, "Road already exists", 0};
    }
    
    runDijkstra();
    lastTrace_ = "Road added between " + std::to_string(a) + " and " + std::to_string(b) + " | Dijkstra rerun from depot";
    return {Err::None, "Road added", 0};
}

OpResult Engine::removeRoad(int a, int b) {
    if (!graph_.removeEdge(a, b)) {
        return {Err::NotFound, "Road not found", 0};
    }
    runDijkstra();
    lastTrace_ = "Road removed between " + std::to_string(a) + " and " + std::to_string(b) + " | Dijkstra rerun from depot";
    return {Err::None, "Road removed", 0};
}

OpResult Engine::setRoadStatus(int a, int b, RoadStatus s) {
    if (!graph_.setStatus(a, b, s)) {
        return {Err::NotFound, "Road not found", 0};
    }
    runDijkstra();
    std::string sStr = (s == RoadStatus::Open) ? "open" : (s == RoadStatus::Slow) ? "slow" : "blocked";
    lastTrace_ = "Road " + std::to_string(a) + "-" + std::to_string(b) + " set to " + sStr + " | Dijkstra rerun from depot";
    return {Err::None, "Road status updated", 0};
}

OpResult Engine::setAverageSpeed(double kmh) {
    if (!graph_.setAverageSpeed(kmh)) {
        return {Err::Invalid, "Speed must be a finite number > 0", 0};
    }
    runDijkstra();
    lastTrace_ = "Average speed set to " + std::to_string(kmh) + " | Dijkstra rerun from depot";
    return {Err::None, "Speed updated", 0};
}

OpResult Engine::submitRequest(int campId, ResourceType r, int quantity, int urgency, int families) {
    Request req;
    req.id = nextRequestId_;
    req.campId = campId;
    req.resource = r;
    req.quantity = quantity;
    req.urgency = urgency;
    req.families = families;
    req.arrivalSeq = nextArrivalSeq_;
    
    if (!isValidRequest(req)) {
        return {Err::Invalid, "Invalid request details", 0};
    }
    Camp tmpC;
    if (!camps_.search(campId, tmpC)) {
        return {Err::NotFound, "Camp not found", 0};
    }
    
    queue_.enqueue(req);
    nextRequestId_++;
    nextArrivalSeq_++;
    
    lastTrace_ = "Queue enqueue: request " + std::to_string(req.id) + " for camp " + std::to_string(campId);
    return {Err::None, "Request queued", req.id};
}

OpResult Engine::processIncoming() {
    if (queue_.isEmpty()) {
        return {Err::Empty, "Queue is empty", 0};
    }
    
    int count = 0;
    while (!queue_.isEmpty()) {
        Request req;
        queue_.dequeue(req);
        activeRequests_.insert(req);
        count++;
    }
    
    activeRequests_.sortByPriority();
    lastTrace_ = "Process incoming: " + std::to_string(count) + " moved to the active list, merge sort run";
    return {Err::None, "Requests processed", count};
}

OpResult Engine::updateRequest(int requestId, int urgency, int families, int quantity) {
    Request req;
    if (!activeRequests_.searchById(requestId, req)) {
        return {Err::NotFound, "Request not found in active list (process incoming first)", 0};
    }
    
    if (!activeRequests_.update(requestId, urgency, families, quantity)) {
        return {Err::Invalid, "Invalid values", 0};
    }
    
    activeRequests_.sortByPriority();
    lastTrace_ = "Update request: " + std::to_string(requestId) + " updated, merge sort run";
    return {Err::None, "Request updated", 0};
}

OpResult Engine::handleNext() {
    if (activeRequests_.isEmpty()) {
        return {Err::Empty, "No active requests", 0};
    }
    
    runDijkstra();
    
    std::vector<Request> active = activeRequests_.toVector();
    Request targetReq;
    bool foundServiceable = false;
    bool sawOutOfStock = false;
    std::string firstOutOfStockName;

    auto stockOf = [this](ResourceType t) {
        return t == ResourceType::Food ? stock_.food
             : t == ResourceType::Water ? stock_.water
             : stock_.medicine;
    };
    auto nameOf = [](ResourceType t) -> const char* {
        return t == ResourceType::Food ? "food"
             : t == ResourceType::Water ? "water"
             : "medicine";
    };
    
    for (const auto& r : active) {
        // Serve the first request in ranked order that can be served right now:
        // its camp must be reachable AND the depot must still hold some of its resource.
        // A request that cannot be served is skipped, so it never blocks the ones behind it.
        if (!dijkstra_.isReachable(r.campId)) continue;
        if (stockOf(r.resource) == 0) {
            if (!sawOutOfStock) {
                sawOutOfStock = true;
                firstOutOfStockName = nameOf(r.resource);
            }
            continue;
        }
        targetReq = r;
        foundServiceable = true;
        break;
    }
    
    if (!foundServiceable) {
        // Nothing can be served. If at least one request had a reachable camp but no stock,
        // report the stock-out (it is the more useful fact); otherwise every camp is cut off.
        if (sawOutOfStock) {
            return {Err::OutOfStock, "Out of stock for " + firstOutOfStockName, 0};
        }
        return {Err::Unreachable, "Unreachable: alternate delivery needed", 0};
    }
    
    // Never 0 here: the loop above skips every request whose resource is out of stock.
    int stockAvailable = stockOf(targetReq.resource);

    int delivered = (targetReq.quantity < stockAvailable) ? targetReq.quantity : stockAvailable;
    int mins = dijkstra_.getDistance(targetReq.campId);
    std::vector<int> path;
    dijkstra_.reconstructPath(targetReq.campId, path);
    
    if (targetReq.resource == ResourceType::Food) stock_.food -= delivered;
    else if (targetReq.resource == ResourceType::Water) stock_.water -= delivered;
    else stock_.medicine -= delivered;
    
    camps_.addReceived(targetReq.campId, targetReq.resource, delivered);
    
    DeliveryRecord dr;
    dr.requestId = targetReq.id;
    dr.campId = targetReq.campId;
    dr.resource = targetReq.resource;
    dr.quantityRequested = targetReq.quantity;
    dr.quantityDelivered = delivered;
    dr.totalMinutes = mins;
    dr.path = path;
    log_.append(dr);
    
    std::string resMsg = "Handle next: request " + std::to_string(targetReq.id) + " to camp " + std::to_string(targetReq.campId) + ", route ";
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) resMsg += " -> ";
        resMsg += std::to_string(path[i]);
    }
    resMsg += ", " + std::to_string(mins) + " min, delivered " + std::to_string(delivered) + " of " + std::to_string(targetReq.quantity);
    
    if (delivered == targetReq.quantity) {
        activeRequests_.deleteById(targetReq.id);
    } else {
        activeRequests_.update(targetReq.id, targetReq.urgency, targetReq.families, targetReq.quantity - delivered);
        activeRequests_.sortByPriority();
        resMsg += " (residual)";
    }
    
    lastTrace_ = resMsg;
    return {Err::None, resMsg, delivered};
}

OpResult Engine::setStock(int food, int water, int medicine) {
    if (food < 0 || water < 0 || medicine < 0) {
        return {Err::Invalid, "Stock cannot be negative", 0};
    }
    stock_.food = food;
    stock_.water = water;
    stock_.medicine = medicine;
    lastTrace_ = "Stock set.";
    return {Err::None, "Stock updated", 0};
}

std::vector<Request> Engine::queueContents() const {
    return queue_.toVector();
}

std::vector<RankedRequest> Engine::rankedRequests() const {
    std::vector<Request> list = activeRequests_.toVector();
    std::vector<RankedRequest> ranked;
    for (size_t i = 0; i < list.size(); ++i) {
        RankedRequest rr;
        rr.req = list[i];
        if (i + 1 < list.size()) {
            rr.hasReason = true;
            rr.reason = rankReason(list[i], list[i+1]);
        } else {
            rr.hasReason = false;
        }
        ranked.push_back(rr);
    }
    return ranked;
}

std::vector<DeliveryRecord> Engine::deliveries() const {
    return log_.toVector();
}

std::vector<RouteInfo> Engine::routes() const {
    // Spec: "routes(): for every camp in ascending id, use the latest Dijkstra result."
    // Graph::nodeIds gives ascending ids, including 0.
    std::vector<int> ids;
    graph_.nodeIds(ids);
    
    std::vector<RouteInfo> result;
    for (int id : ids) {
        if (id == 0) continue;
        RouteInfo info;
        info.campId = id;
        info.reachable = dijkstra_.isReachable(id);
        info.minutes = dijkstra_.getDistance(id);
        dijkstra_.reconstructPath(id, info.path);
        result.push_back(info);
    }
    return result;
}

std::vector<EdgeInfo> Engine::roads() const {
    std::vector<EdgeInfo> edges;
    graph_.edgeList(edges);
    return edges;
}

void Engine::avlSnapshot(std::vector<AVLNodeInfo>& out) const {
    camps_.snapshot(out);
}

const std::vector<RotationEvent>& Engine::lastRotations() const {
    return camps_.lastRotations();
}

const Dijkstra& Engine::dijkstra() const {
    return dijkstra_;
}

Stock Engine::stock() const {
    return stock_;
}

double Engine::averageSpeed() const {
    return graph_.averageSpeed();
}

double Engine::depotX() const {
    return depotX_;
}

double Engine::depotY() const {
    return depotY_;
}

int Engine::campCount() const {
    return camps_.count();
}

std::vector<Camp> Engine::camps() const {
    return camps_.inorder();
}

bool Engine::findCamp(int id, Camp& out) const {
    return camps_.search(id, out);
}

int Engine::pendingCountForCamp(int campId) const {
    int count = 0;
    std::vector<Request> q = queue_.toVector();
    for (const auto& r : q) {
        if (r.campId == campId) count++;
    }
    std::vector<Request> act;
    activeRequests_.searchByCamp(campId, act);
    count += act.size();
    return count;
}

const std::string& Engine::lastTrace() const {
    return lastTrace_;
}
