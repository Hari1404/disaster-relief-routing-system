#include "Api.h"
#include <climits>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include "json.hpp"

using nlohmann::json;

// ---------------------------------------------------------------------------------------------
// Everything below the line is private to this file.
// ---------------------------------------------------------------------------------------------
namespace {

// The scenario files in data/. A fixed list, so a request can never name any other file.
const std::vector<std::string> kScenarios = {
    "scenario1_normal", "scenario2_blockage", "scenario3_priority", "scenario4_avl"
};

// Same sentence the Engine uses when handle-next finds only unreachable camps.
const char* const kUnreachableNote = "Unreachable: alternate delivery needed";

// Thrown by the input checks. Api::handle turns it into a 400 response.
struct BadRequest {
    std::string message;
};

[[noreturn]] void bad(const std::string& message) {
    throw BadRequest{message};
}

// ---- names -----------------------------------------------------------------------------------

const char* resourceName(ResourceType r) {
    switch (r) {
        case ResourceType::Food:  return "food";
        case ResourceType::Water: return "water";
        default:                  return "medicine";
    }
}

const char* statusName(RoadStatus s) {
    switch (s) {
        case RoadStatus::Open: return "open";
        case RoadStatus::Slow: return "slow";
        default:               return "blocked";
    }
}

const char* errName(Err e) {
    switch (e) {
        case Err::None:          return "None";
        case Err::Invalid:       return "Invalid";
        case Err::Duplicate:     return "Duplicate";
        case Err::NotFound:      return "NotFound";
        case Err::ConfirmNeeded: return "ConfirmNeeded";
        case Err::Empty:         return "Empty";
        case Err::Unreachable:   return "Unreachable";
        default:                 return "OutOfStock";
    }
}

// Invalid input is a 400, a missing thing is a 404, a clash with the current state is a 409.
// Empty, Unreachable and OutOfStock are valid requests that the current state cannot serve.
int statusFor(Err e) {
    switch (e) {
        case Err::None:      return 200;
        case Err::Invalid:   return 400;
        case Err::NotFound:  return 404;
        default:             return 409;
    }
}

// ---- reading JSON input ----------------------------------------------------------------------

std::string clip(const std::string& s) {
    return s.size() <= 40 ? s : s.substr(0, 40) + "...";
}

json parseObject(const std::string& body) {
    json j = json::parse(body, nullptr, false);   // false: no exception on bad text
    if (j.is_discarded()) bad("Request body is not valid JSON");
    if (!j.is_object())   bad("Request body must be a JSON object");
    return j;
}

// Refuses any key that is not in the list, so a misspelt optional field is not silently ignored.
void allowOnly(const json& o, std::initializer_list<const char*> keys) {
    for (auto it = o.begin(); it != o.end(); ++it) {
        bool known = false;
        for (const char* k : keys) {
            if (it.key() == k) known = true;
        }
        if (!known) bad("Unknown field '" + clip(it.key()) + "'");
    }
}

const json& need(const json& o, const char* key) {
    auto it = o.find(key);
    if (it == o.end()) bad(std::string("Missing field '") + key + "'");
    return *it;
}

// A whole number that fits in an int. Rejects 3.5, 3.0, "3", true, null and 99999999999.
int toInt(const json& v, const std::string& what) {
    if (!v.is_number_integer()) bad(what + " must be a whole number");
    if (v.is_number_unsigned()) {
        unsigned long long u = v.get<unsigned long long>();
        if (u > static_cast<unsigned long long>(INT_MAX)) bad(what + " is out of range");
        return static_cast<int>(u);
    }
    long long s = v.get<long long>();
    if (s < INT_MIN || s > INT_MAX) bad(what + " is out of range");
    return static_cast<int>(s);
}

int intField(const json& o, const char* key) {
    return toInt(need(o, key), std::string("Field '") + key + "'");
}

// Any JSON number. (JSON has no NaN or infinity, and the Engine checks the range.)
double numField(const json& o, const char* key) {
    const json& v = need(o, key);
    if (!v.is_number()) bad(std::string("Field '") + key + "' must be a number");
    return v.get<double>();
}

bool boolField(const json& o, const char* key) {
    const json& v = need(o, key);
    if (!v.is_boolean()) bad(std::string("Field '") + key + "' must be true or false");
    return v.get<bool>();
}

std::string textField(const json& o, const char* key) {
    const json& v = need(o, key);
    if (!v.is_string()) bad(std::string("Field '") + key + "' must be text");
    return v.get<std::string>();
}

// A camp name: spaces at both ends are trimmed, then 1 to 40 bytes, no control characters.
std::string nameField(const json& o, const char* key) {
    std::string s = textField(o, key);
    std::size_t first = s.find_first_not_of(' ');
    if (first == std::string::npos) bad("Camp name must not be empty");
    std::size_t last = s.find_last_not_of(' ');
    s = s.substr(first, last - first + 1);
    if (s.size() > 40) bad("Camp name must be at most 40 characters");
    for (std::size_t i = 0; i < s.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch < 0x20 || ch == 0x7F) bad("Camp name must not contain control characters");
    }
    return s;
}

// "links" is optional. When present it must be an array of whole numbers (at most 200).
std::vector<int> linksField(const json& o) {
    std::vector<int> out;
    auto it = o.find("links");
    if (it == o.end()) return out;
    if (!it->is_array()) bad("Field 'links' must be an array of camp ids");
    if (it->size() > 200) bad("Field 'links' may hold at most 200 ids");
    for (const json& v : *it) out.push_back(toInt(v, "Each entry in 'links'"));
    return out;
}

// Reads a whole number from the URL, for example the 5 in /api/camps/5.
int idFromPath(const std::string& text, const char* what) {
    int id = 0;
    if (!parseWholeNumber(text, id)) bad(std::string(what) + " must be a whole number");
    return id;
}

std::vector<std::string> splitSegments(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        std::size_t slash = s.find('/', start);
        if (slash == std::string::npos) {
            out.push_back(s.substr(start));
            return out;
        }
        out.push_back(s.substr(start, slash - start));
        start = slash + 1;
    }
}

// ---- building JSON output --------------------------------------------------------------------

std::string toText(const json& j) {
    // "replace": a stray non-UTF-8 byte (say, from a hand-edited scenario file) becomes U+FFFD
    // instead of making dump() throw.
    return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

json requestJson(const Request& r) {
    json j;
    j["id"]         = r.id;
    j["campId"]     = r.campId;
    j["resource"]   = resourceName(r.resource);
    j["quantity"]   = r.quantity;
    j["urgency"]    = r.urgency;
    j["families"]   = r.families;
    j["arrivalSeq"] = r.arrivalSeq;
    return j;
}

// Why row a ranks above the row below it (b), in plain words. rankReason() names the key,
// the sentence just adds the two values.
std::string reasonText(const Request& a, const Request& b, RankKey k) {
    switch (k) {
        case RankKey::Urgency:
            return "urgency " + std::to_string(a.urgency) + " > " + std::to_string(b.urgency);
        case RankKey::Families:
            return "same urgency, families " + std::to_string(a.families) + " > " + std::to_string(b.families);
        case RankKey::Quantity:
            return "same urgency and families, quantity " + std::to_string(a.quantity) + " > " + std::to_string(b.quantity);
        default:
            return "all else equal, arrived earlier (#" + std::to_string(a.arrivalSeq) + " before #" + std::to_string(b.arrivalSeq) + ")";
    }
}

json campJson(const Engine& e, const Camp& c) {
    json j;
    j["id"]       = c.id;
    j["name"]     = c.name;
    j["x"]        = c.x;
    j["y"]        = c.y;
    j["families"] = c.families;
    j["received"] = {{"food", c.receivedFood}, {"water", c.receivedWater}, {"medicine", c.receivedMedicine}};
    j["pending"]  = e.pendingCountForCamp(c.id);   // requests waiting in the queue or the active list
    return j;
}

json routeJson(const RouteInfo& r) {
    json j;
    j["campId"]    = r.campId;
    j["reachable"] = r.reachable;
    j["minutes"]   = r.reachable ? json(r.minutes) : json(nullptr);
    j["path"]      = r.path;
    if (!r.reachable) j["note"] = kUnreachableNote;
    return j;
}

json deliveryJson(const DeliveryRecord& d) {
    json j;
    j["requestId"]         = d.requestId;
    j["campId"]            = d.campId;
    j["resource"]          = resourceName(d.resource);
    j["quantityRequested"] = d.quantityRequested;
    j["quantityDelivered"] = d.quantityDelivered;
    j["totalMinutes"]      = d.totalMinutes;
    j["path"]              = d.path;
    return j;
}

// The full snapshot. Every field comes from one of the Engine's read-only views.
json buildState(const Engine& e) {
    json s = json::object();

    s["depot"]    = {{"x", e.depotX()}, {"y", e.depotY()}};
    s["speedKmh"] = e.averageSpeed();
    Stock st = e.stock();
    s["stock"]    = {{"food", st.food}, {"water", st.water}, {"medicine", st.medicine}};
    s["trace"]    = e.lastTrace();

    json camps = json::array();
    for (const Camp& c : e.camps()) camps.push_back(campJson(e, c));
    s["camps"] = camps;

    json roads = json::array();
    for (const EdgeInfo& r : e.roads()) {
        roads.push_back({{"a", r.a}, {"b", r.b}, {"distanceKm", r.distanceKm},
                         {"status", statusName(r.status)}, {"minutes", r.minutes}});
    }
    s["roads"] = roads;

    json routes = json::array();
    for (const RouteInfo& r : e.routes()) routes.push_back(routeJson(r));
    s["routes"] = routes;

    json queue = json::array();
    for (const Request& r : e.queueContents()) queue.push_back(requestJson(r));
    s["queue"] = queue;

    std::vector<RankedRequest> ranked = e.rankedRequests();
    json active = json::array();
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        json row = requestJson(ranked[i].req);
        row["rank"] = static_cast<int>(i) + 1;
        if (ranked[i].hasReason) {
            row["reason"]     = rankKeyName(ranked[i].reason);
            row["reasonText"] = reasonText(ranked[i].req, ranked[i + 1].req, ranked[i].reason);
        } else {
            row["reason"]     = nullptr;
            row["reasonText"] = nullptr;
        }
        active.push_back(row);
    }
    s["active"] = active;

    json log = json::array();
    for (const DeliveryRecord& d : e.deliveries()) log.push_back(deliveryJson(d));
    s["log"] = log;

    std::vector<AVLNodeInfo> nodes;
    e.avlSnapshot(nodes);
    json avlNodes = json::array();
    for (const AVLNodeInfo& n : nodes) {
        avlNodes.push_back({{"id", n.id}, {"height", n.height}, {"balance", n.balance},
                            {"depth", n.depth}, {"side", std::string(1, n.side)},
                            {"leftId", n.leftId}, {"rightId", n.rightId}});
    }
    json rotations = json::array();
    for (const RotationEvent& r : e.lastRotations()) {
        rotations.push_back({{"type", rotationName(r.type)}, {"nodeId", r.nodeId}});
    }
    json avl;
    avl["nodes"]     = avlNodes;                              // preorder: the root comes first
    avl["rootId"]    = nodes.empty() ? 0 : nodes[0].id;       // 0 means an empty tree
    avl["height"]    = nodes.empty() ? 0 : nodes[0].height;
    avl["rotations"] = rotations;                             // from the latest AVL insert or delete
    s["avl"] = avl;

    json steps = json::array();
    for (const DijkstraStep& st2 : e.dijkstra().steps()) {
        steps.push_back({{"node", st2.node}, {"minutes", st2.minutes}, {"improved", st2.improved}});
    }
    s["dijkstra"] = {{"source", 0}, {"steps", steps}};

    s["scenarios"] = Api::scenarioNames();
    return s;
}

// One response in the common shape: ok, error (null on success), message, any extra fields,
// and the full state. The state is included on failures too, so the page can always redraw.
ApiResponse reply(const Engine& e, int status, bool ok, const char* error, const std::string& message,
                  const json& extra = json::object(), bool withState = true) {
    json out = json::object();
    out["ok"]      = ok;
    out["error"]   = error ? json(error) : json(nullptr);
    out["message"] = message;
    for (auto it = extra.begin(); it != extra.end(); ++it) out[it.key()] = it.value();
    if (withState) out["state"] = buildState(e);
    return {status, toText(out)};
}

// Turns an Engine result into a response. valueKey names the Engine's `value` on success.
ApiResponse fromOp(const Engine& e, const OpResult& r, const char* valueKey) {
    json extra = json::object();
    if (r.ok() && valueKey) extra[valueKey] = r.value;
    if (r.err == Err::ConfirmNeeded) {
        extra["needsConfirm"] = true;
        extra["pending"]      = r.value;
    }
    return reply(e, statusFor(r.err), r.ok(), r.ok() ? nullptr : errName(r.err), r.message, extra);
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// ---- the endpoints ---------------------------------------------------------------------------

struct Ctx {
    Engine&           e;
    const ApiRequest& req;
    const std::string& dataDir;
};

ApiResponse methodNotAllowed(Ctx& c, const char* allowed) {
    return reply(c.e, 405, false, "MethodNotAllowed", std::string("Use ") + allowed + " for this path",
                 json::object(), false);
}

// GET /api/state
ApiResponse getState(Ctx& c) {
    return reply(c.e, 200, true, nullptr, "State");
}

// POST /api/camps   { id, name, x, y, families, links?: [ids] }
ApiResponse postCamp(Ctx& c) {
    json b = parseObject(c.req.body);
    allowOnly(b, {"id", "name", "x", "y", "families", "links"});
    Camp camp;
    camp.id               = intField(b, "id");
    camp.name             = nameField(b, "name");
    camp.x                = numField(b, "x");
    camp.y                = numField(b, "y");
    camp.families         = intField(b, "families");
    camp.receivedFood     = 0;
    camp.receivedWater    = 0;
    camp.receivedMedicine = 0;
    std::vector<int> links = linksField(b);
    return fromOp(c.e, c.e.addCamp(camp, links), "roadsAdded");
}

// GET /api/camps/{id}   the AVL search, plus the path the search takes from the root
ApiResponse getCamp(Ctx& c, int id) {
    Camp camp;
    if (!c.e.findCamp(id, camp)) {
        return reply(c.e, 404, false, "NotFound", "Camp not found", json::object(), false);
    }
    // Walk down from the root the way AVLTree::search does, using the snapshot's child ids.
    std::vector<AVLNodeInfo> nodes;
    c.e.avlSnapshot(nodes);
    json path = json::array();
    int cur = nodes.empty() ? 0 : nodes[0].id;
    while (cur != 0) {
        path.push_back(cur);
        if (cur == id) break;
        int next = 0;
        for (const AVLNodeInfo& n : nodes) {
            if (n.id == cur) {
                next = (id < cur) ? n.leftId : n.rightId;
                break;
            }
        }
        cur = next;
    }
    json extra;
    extra["camp"]       = campJson(c.e, camp);
    extra["searchPath"] = path;
    for (const RouteInfo& r : c.e.routes()) {
        if (r.campId == id) extra["route"] = routeJson(r);
    }
    return reply(c.e, 200, true, nullptr, "Camp found", extra, false);
}

// PUT /api/camps/{id}   { name, x, y, families }
ApiResponse putCamp(Ctx& c, int id) {
    json b = parseObject(c.req.body);
    allowOnly(b, {"name", "x", "y", "families"});
    std::string name = nameField(b, "name");
    double x         = numField(b, "x");
    double y         = numField(b, "y");
    int families     = intField(b, "families");
    return fromOp(c.e, c.e.editCamp(id, name, x, y, families), nullptr);
}

// DELETE /api/camps/{id}?confirm=true     (or { "confirm": true } in the body)
ApiResponse deleteCamp(Ctx& c, int id) {
    bool confirm = false;
    for (const auto& kv : c.req.query) {
        if (kv.first != "confirm") bad("Unknown query parameter '" + clip(kv.first) + "'");
        if (kv.second == "true")        confirm = true;
        else if (kv.second != "false")  bad("Query parameter 'confirm' must be true or false");
    }
    if (!c.req.body.empty()) {
        json b = parseObject(c.req.body);
        allowOnly(b, {"confirm"});
        if (boolField(b, "confirm")) confirm = true;
    }
    return fromOp(c.e, c.e.deleteCamp(id, confirm), "cancelledRequests");
}

// POST /api/roads   { a, b }
ApiResponse postRoad(Ctx& c) {
    json b = parseObject(c.req.body);
    allowOnly(b, {"a", "b"});
    int a = intField(b, "a");
    int z = intField(b, "b");
    return fromOp(c.e, c.e.addRoad(a, z), nullptr);
}

// PUT /api/roads   { a, b, status: "open" | "slow" | "blocked" }
ApiResponse putRoad(Ctx& c) {
    json b = parseObject(c.req.body);
    allowOnly(b, {"a", "b", "status"});
    int a = intField(b, "a");
    int z = intField(b, "b");
    std::string word = textField(b, "status");
    RoadStatus s = RoadStatus::Open;
    if (word == "open")         s = RoadStatus::Open;
    else if (word == "slow")    s = RoadStatus::Slow;
    else if (word == "blocked") s = RoadStatus::Blocked;
    else bad("Field 'status' must be \"open\", \"slow\" or \"blocked\"");
    return fromOp(c.e, c.e.setRoadStatus(a, z, s), nullptr);
}

// DELETE /api/roads   { a, b }
ApiResponse deleteRoad(Ctx& c) {
    json b = parseObject(c.req.body);
    allowOnly(b, {"a", "b"});
    int a = intField(b, "a");
    int z = intField(b, "b");
    return fromOp(c.e, c.e.removeRoad(a, z), nullptr);
}

// POST /api/requests   { campId, resource: "food" | "water" | "medicine", quantity, urgency, families }
ApiResponse postRequest(Ctx& c) {
    json b = parseObject(c.req.body);
    allowOnly(b, {"campId", "resource", "quantity", "urgency", "families"});
    int campId       = intField(b, "campId");
    std::string word = textField(b, "resource");
    ResourceType res = ResourceType::Food;
    if (word == "food")          res = ResourceType::Food;
    else if (word == "water")    res = ResourceType::Water;
    else if (word == "medicine") res = ResourceType::Medicine;
    else bad("Field 'resource' must be \"food\", \"water\" or \"medicine\"");
    int quantity = intField(b, "quantity");
    int urgency  = intField(b, "urgency");
    int families = intField(b, "families");
    return fromOp(c.e, c.e.submitRequest(campId, res, quantity, urgency, families), "requestId");
}

// PUT /api/requests/{id}   { urgency, families, quantity }   (the request must be in the active list)
ApiResponse putRequest(Ctx& c, int id) {
    json b = parseObject(c.req.body);
    allowOnly(b, {"urgency", "families", "quantity"});
    int urgency  = intField(b, "urgency");
    int families = intField(b, "families");
    int quantity = intField(b, "quantity");
    return fromOp(c.e, c.e.updateRequest(id, urgency, families, quantity), nullptr);
}

// POST /api/settings   { speedKmh }
ApiResponse postSettings(Ctx& c) {
    json b = parseObject(c.req.body);
    allowOnly(b, {"speedKmh"});
    double kmh = numField(b, "speedKmh");
    return fromOp(c.e, c.e.setAverageSpeed(kmh), nullptr);
}

// POST /api/scenario/{name}
ApiResponse postScenario(Ctx& c, const std::string& name) {
    bool known = false;
    for (const std::string& s : kScenarios) {
        if (s == name) known = true;
    }
    if (!known) {
        return reply(c.e, 404, false, "NotFound", "Unknown scenario '" + clip(name) + "'");
    }
    // Read the file BEFORE touching the Engine: loadScenario() clears everything first, so a
    // missing file must not be allowed to wipe the session.
    std::string text;
    if (!readFile(c.dataDir + "/" + name + ".txt", text)) {
        return reply(c.e, 500, false, "ServerError",
                     "Cannot read " + c.dataDir + "/" + name + ".txt. Start the server from the repository root.");
    }
    return fromOp(c.e, c.e.loadScenario(text), "requestsQueued");
}

ApiResponse route(Ctx& c) {
    const std::string& path = c.req.path;
    const std::string& m    = c.req.method;
    const std::string prefix = "/api/";
    if (path.compare(0, prefix.size(), prefix) != 0) {
        return reply(c.e, 404, false, "NotFound", "No such endpoint", json::object(), false);
    }
    std::vector<std::string> seg = splitSegments(path.substr(prefix.size()));
    const std::string& s0 = seg[0];

    if (s0 == "state" && seg.size() == 1) {
        if (m == "GET") return getState(c);
        return methodNotAllowed(c, "GET");
    }
    if (s0 == "camps" && seg.size() == 1) {
        if (m == "POST") return postCamp(c);
        return methodNotAllowed(c, "POST");
    }
    if (s0 == "camps" && seg.size() == 2) {
        if (m != "GET" && m != "PUT" && m != "DELETE") return methodNotAllowed(c, "GET, PUT or DELETE");
        int id = idFromPath(seg[1], "Camp id");
        if (m == "GET") return getCamp(c, id);
        if (m == "PUT") return putCamp(c, id);
        return deleteCamp(c, id);
    }
    if (s0 == "roads" && seg.size() == 1) {
        if (m == "POST")   return postRoad(c);
        if (m == "PUT")    return putRoad(c);
        if (m == "DELETE") return deleteRoad(c);
        return methodNotAllowed(c, "POST, PUT or DELETE");
    }
    if (s0 == "requests" && seg.size() == 1) {
        if (m == "POST") return postRequest(c);
        return methodNotAllowed(c, "POST");
    }
    if (s0 == "requests" && seg.size() == 2) {
        if (m != "PUT") return methodNotAllowed(c, "PUT");
        return putRequest(c, idFromPath(seg[1], "Request id"));
    }
    if (s0 == "process" && seg.size() == 1) {
        if (m != "POST") return methodNotAllowed(c, "POST");
        return fromOp(c.e, c.e.processIncoming(), "moved");
    }
    if (s0 == "handle-next" && seg.size() == 1) {
        if (m != "POST") return methodNotAllowed(c, "POST");
        return fromOp(c.e, c.e.handleNext(), "delivered");
    }
    if (s0 == "settings" && seg.size() == 1) {
        if (m == "POST") return postSettings(c);
        return methodNotAllowed(c, "POST");
    }
    if (s0 == "scenario" && seg.size() == 2) {
        if (m == "POST") return postScenario(c, seg[1]);
        return methodNotAllowed(c, "POST");
    }
    if (s0 == "reset" && seg.size() == 1) {
        if (m != "POST") return methodNotAllowed(c, "POST");
        c.e.reset();
        return reply(c.e, 200, true, nullptr, "Reset");
    }
    return reply(c.e, 404, false, "NotFound", "No such endpoint", json::object(), false);
}

// A browser sends an Origin header on every cross-site POST/PUT/DELETE. Only this server's own
// page may change the state, so another web page open in the same browser cannot press
// "reset" for the user. Tools such as curl send no Origin header and are not affected.
bool originAllowed(const std::string& origin, int port) {
    std::string p = std::to_string(port);
    return origin == "http://localhost:" + p || origin == "http://127.0.0.1:" + p;
}

} // namespace

// ---------------------------------------------------------------------------------------------

Api::Api(Engine& engine, const std::string& dataDir, int port)
    : engine_(engine), dataDir_(dataDir), port_(port) {
}

const std::vector<std::string>& Api::scenarioNames() {
    return kScenarios;
}

ApiResponse Api::handle(const ApiRequest& req) {
    try {
        if (req.method != "GET" && !req.origin.empty() && !originAllowed(req.origin, port_)) {
            return reply(engine_, 403, false, "Forbidden", "Cross-origin request refused", json::object(), false);
        }
        Ctx c{engine_, req, dataDir_};
        return route(c);
    } catch (const BadRequest& b) {
        try {
            return reply(engine_, 400, false, "BadRequest", b.message);
        } catch (...) {
            // fall through to the plain answer below
        }
    } catch (...) {
        // fall through
    }
    return {500, "{\"ok\":false,\"error\":\"ServerError\",\"message\":\"Internal error\"}"};
}
