// Tests for the server's API layer (server/Api.cpp). No sockets: Api::handle is called directly.
//
//   make test-server        (or build_server_tests.bat on Windows)
//
// Run from the repository root: the scenario files are read from data/.
// These tests need server/json.hpp (see README.md); the main test suite does not.

#include <climits>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <string>
#include <vector>
#include "test_helpers.h"
#include "Api.h"
#include "Engine.h"
#include "json.hpp"

using nlohmann::json;

int g_checks   = 0;
int g_failures = 0;

// ---- helpers ---------------------------------------------------------------------------------

struct Reply {
    int         status;
    json        body;
    std::string text;
    const json& state() const { return body.at("state"); }
    bool        ok() const { return body.at("ok").get<bool>(); }
    std::string error() const { return body.at("error").is_null() ? "" : body.at("error").get<std::string>(); }
    std::string message() const { return body.at("message").get<std::string>(); }
};

// target may carry a query string: "/api/camps/5?confirm=true".
static Reply call(Api& api, const std::string& method, const std::string& target,
                  const std::string& body = "", const std::string& origin = "") {
    ApiRequest r;
    r.method = method;
    std::size_t q = target.find('?');
    r.path = target.substr(0, q);
    if (q != std::string::npos) {
        std::string rest = target.substr(q + 1);
        std::size_t start = 0;
        while (start <= rest.size()) {
            std::size_t amp = rest.find('&', start);
            std::string pair = rest.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            std::size_t eq = pair.find('=');
            r.query.push_back({pair.substr(0, eq), eq == std::string::npos ? "" : pair.substr(eq + 1)});
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }
    r.body   = body;
    r.origin = origin;
    ApiResponse out = api.handle(r);

    Reply rep;
    rep.status = out.status;
    rep.text   = out.body;
    CHECK(json::accept(out.body));            // every answer, even an error, is valid JSON
    rep.body   = json::parse(out.body, nullptr, false);
    if (rep.body.is_discarded()) rep.body = json::object({{"ok", false}, {"error", "NotJson"}, {"message", out.body}});
    return rep;
}

static void addCamp(Api& api, int id, double x, double y, const std::string& links = "[]") {
    json b = {{"id", id}, {"name", "C" + std::to_string(id)}, {"x", x}, {"y", y}, {"families", 10}};
    b["links"] = json::parse(links);
    Reply r = call(api, "POST", "/api/camps", b.dump());
    CHECK(r.status == 200 && r.ok());
}

static Reply loadScenario(Api& api, const std::string& name) {
    return call(api, "POST", "/api/scenario/" + name);
}

static std::vector<int> ids(const json& arrayOfObjects, const char* key) {
    std::vector<int> out;
    for (const json& o : arrayOfObjects) out.push_back(o.at(key).get<int>());
    return out;
}

static bool sameInts(const std::vector<int>& a, std::initializer_list<int> b) {
    return a == std::vector<int>(b);
}

// ---- 1. snapshot of an empty engine ----------------------------------------------------------

static void testEmptyState() {
    Engine e;
    Api api(e, "data", 8080);
    Reply r = call(api, "GET", "/api/state");
    CHECK(r.status == 200 && r.ok() && r.error() == "");
    const json& s = r.state();
    CHECK(s.at("depot").at("x") == 0 && s.at("depot").at("y") == 0);
    CHECK(s.at("speedKmh") == 40.0);
    CHECK(s.at("stock").at("food") == 500 && s.at("stock").at("water") == 500 && s.at("stock").at("medicine") == 500);
    CHECK(s.at("trace") == "Reset.");
    CHECK(s.at("camps").empty() && s.at("roads").empty() && s.at("routes").empty());
    CHECK(s.at("queue").empty() && s.at("active").empty() && s.at("log").empty());
    CHECK(s.at("avl").at("rootId") == 0 && s.at("avl").at("height") == 0 && s.at("avl").at("nodes").empty());
    CHECK(s.at("dijkstra").at("source") == 0);
    CHECK(s.at("dijkstra").at("steps").size() == 1);              // only the depot is picked
    CHECK(s.at("scenarios").size() == 4);
    CHECK(s.at("scenarios")[0] == "scenario1_normal" && s.at("scenarios")[3] == "scenario4_avl");

    // Reading changes nothing: two GETs give the same text.
    CHECK(call(api, "GET", "/api/state").text == r.text);
}

// ---- 2. scenario 1 through the API, against the hand-checked expected file --------------------

static void testScenario1() {
    Engine e;
    Api api(e, "data", 8080);
    Reply r = loadScenario(api, "scenario1_normal");
    CHECK(r.status == 200 && r.ok() && r.body.at("requestsQueued") == 6);
    const json& s = r.state();
    CHECK(s.at("camps").size() == 6 && s.at("roads").size() == 10 && s.at("queue").size() == 6);
    CHECK(s.at("speedKmh") == 60.0 && s.at("stock").at("food") == 1000);
    CHECK(std::string(s.at("trace").get<std::string>()).find("Scenario loaded: 6 camps, 10 roads, 6 requests queued") == 0);

    // routes: 5, 6, 10, 11, 12, 16 minutes; camp 6 goes 0 -> 1 -> 3 -> 6
    const json& routes = s.at("routes");
    CHECK(routes.size() == 6);
    int expectMin[6] = {5, 6, 10, 11, 12, 16};
    for (int i = 0; i < 6 && i < static_cast<int>(routes.size()); ++i) {
        CHECK(routes[i].at("campId") == i + 1 && routes[i].at("reachable") == true);
        CHECK(routes[i].at("minutes") == expectMin[i]);
    }
    CHECK(routes[5].at("path") == json::array({0, 1, 3, 6}));
    CHECK(routes[4].at("path") == json::array({0, 2, 5}));

    // Dijkstra picks 0, 1, 2, 3, 4, 5, 6
    const json& steps = s.at("dijkstra").at("steps");
    CHECK(steps.size() == 7);
    for (int i = 0; i < 7 && i < static_cast<int>(steps.size()); ++i) CHECK(steps[i].at("node") == i);

    // queue is in arrival order
    CHECK(sameInts(ids(s.at("queue"), "id"), {1, 2, 3, 4, 5, 6}));
    CHECK(s.at("queue")[0].at("resource") == "food" && s.at("queue")[1].at("resource") == "water");

    // Process incoming: ranked 3, 6, 2, 4, 1, 5 with reasons arrival, families, urgency, quantity, urgency
    r = call(api, "POST", "/api/process");
    CHECK(r.status == 200 && r.ok() && r.body.at("moved") == 6);
    const json& act = r.state().at("active");
    CHECK(r.state().at("queue").empty());
    CHECK(sameInts(ids(act, "id"), {3, 6, 2, 4, 1, 5}));
    const char* reasons[5] = {"arrival", "families", "urgency", "quantity", "urgency"};
    for (int i = 0; i < 5 && i < static_cast<int>(act.size()); ++i) CHECK(act[i].at("reason") == reasons[i]);
    CHECK(act[5].at("reason").is_null() && act[5].at("reasonText").is_null());
    CHECK(act[0].at("rank") == 1 && act[5].at("rank") == 6);
    // request 3 over 6: identical on every key, so arrival decides. 6 over 2: both urgency 4, families 40 > 10.
    // 2 over 4: urgency 4 > 2. 4 over 1: urgency and families tie, quantity 80 > 50. 1 over 5: urgency 2 > 1.
    CHECK(act[0].at("reasonText") == "all else equal, arrived earlier (#3 before #6)");
    CHECK(act[1].at("reasonText") == "same urgency, families 40 > 10");
    CHECK(act[2].at("reasonText") == "urgency 4 > 2");
    CHECK(act[3].at("reasonText") == "same urgency and families, quantity 80 > 50");
    CHECK(act[4].at("reasonText") == "urgency 2 > 1");
}

// ---- 3. handle-next, full run of scenario 1 ---------------------------------------------------

static void testHandleNextScenario1() {
    Engine e;
    Api api(e, "data", 8080);
    loadScenario(api, "scenario1_normal");

    // Before Process incoming there is nothing to handle.
    Reply r = call(api, "POST", "/api/handle-next");
    CHECK(r.status == 409 && !r.ok() && r.error() == "Empty");
    CHECK(r.state().at("queue").size() == 6);                  // failure still returns the state

    call(api, "POST", "/api/process");
    int delivered[6]   = {20, 20, 200, 80, 50, 500};
    int minutes[6]     = {10, 6, 6, 5, 5, 11};
    int camp[6]        = {3, 2, 2, 1, 1, 4};
    for (int i = 0; i < 6; ++i) {
        r = call(api, "POST", "/api/handle-next");
        CHECK(r.status == 200 && r.ok());
        CHECK(r.body.at("delivered") == delivered[i]);
        const json& log = r.state().at("log");
        CHECK(static_cast<int>(log.size()) == i + 1);
        if (static_cast<int>(log.size()) == i + 1) {
            CHECK(log[i].at("campId") == camp[i] && log[i].at("totalMinutes") == minutes[i]);
            CHECK(log[i].at("quantityDelivered") == delivered[i]);
        }
    }
    const json& s = r.state();
    CHECK(s.at("log")[0].at("path") == json::array({0, 1, 3}));
    CHECK(s.at("log")[5].at("path") == json::array({0, 1, 4}));
    CHECK(s.at("stock").at("food") == 870 && s.at("stock").at("water") == 300 && s.at("stock").at("medicine") == 960);
    CHECK(s.at("active").empty());
    CHECK(s.at("camps")[0].at("received").at("food") == 130);            // camp 1
    CHECK(s.at("camps")[1].at("received").at("water") == 200 && s.at("camps")[1].at("received").at("medicine") == 20);
    CHECK(s.at("camps")[0].at("pending") == 0);

    r = call(api, "POST", "/api/handle-next");                           // a seventh call
    CHECK(r.status == 409 && r.error() == "Empty" && r.message() == "No active requests");
}

// ---- 4. scenario 2: blockage, unreachable, reopen ---------------------------------------------

static void testScenario2Blockage() {
    Engine e;
    Api api(e, "data", 8080);
    loadScenario(api, "scenario2_blockage");

    Reply r = call(api, "PUT", "/api/roads", R"({"a":3,"b":6,"status":"blocked"})");
    CHECK(r.status == 200 && r.ok());
    CHECK(r.state().at("trace") == "Road 3-6 set to blocked | Dijkstra rerun from depot");
    CHECK(r.state().at("routes")[5].at("minutes") == 20);
    CHECK(r.state().at("routes")[5].at("path") == json::array({0, 2, 5, 6}));
    bool found = false;
    for (const json& road : r.state().at("roads")) {
        if (road.at("a") == 3 && road.at("b") == 6) { found = true; CHECK(road.at("status") == "blocked"); }
    }
    CHECK(found);

    r = call(api, "PUT", "/api/roads", R"({"a":5,"b":6,"status":"blocked"})");
    const json& rt = r.state().at("routes")[5];
    CHECK(rt.at("reachable") == false && rt.at("minutes").is_null() && rt.at("path").empty());
    CHECK(rt.at("note") == "Unreachable: alternate delivery needed");
    CHECK(r.state().at("routes")[0].at("minutes") == 5);                 // the others are unchanged

    call(api, "POST", "/api/process");
    r = call(api, "POST", "/api/handle-next");
    CHECK(r.status == 409 && r.error() == "Unreachable" && r.message() == "Unreachable: alternate delivery needed");
    CHECK(r.state().at("active").size() == 1 && r.state().at("log").empty());

    r = call(api, "PUT", "/api/roads", R"({"a":6,"b":5,"status":"open"})");   // endpoints in either order
    CHECK(r.status == 200 && r.state().at("routes")[5].at("minutes") == 20);
    r = call(api, "POST", "/api/handle-next");
    CHECK(r.status == 200 && r.body.at("delivered") == 40 && r.state().at("stock").at("food") == 960);

    // slow road: same path, bigger total
    r = call(api, "PUT", "/api/roads", R"({"a":0,"b":1,"status":"slow"})");
    CHECK(r.status == 200);
    for (const json& road : r.state().at("roads")) {
        if (road.at("a") == 0 && road.at("b") == 1) CHECK(road.at("status") == "slow" && road.at("minutes") == 10);
    }
}

// ---- 5. scenario 3: priority, partial delivery, out of stock ----------------------------------

static void testScenario3Priority() {
    Engine e;
    Api api(e, "data", 8080);
    loadScenario(api, "scenario3_priority");
    Reply r = call(api, "POST", "/api/process");
    CHECK(sameInts(ids(r.state().at("active"), "id"), {2, 3, 1}));
    CHECK(r.state().at("active")[0].at("reason") == "urgency");

    r = call(api, "POST", "/api/handle-next");
    CHECK(r.body.at("delivered") == 30 && r.state().at("stock").at("food") == 70);
    r = call(api, "POST", "/api/handle-next");
    CHECK(r.body.at("delivered") == 40 && r.state().at("stock").at("water") == 60);
    r = call(api, "POST", "/api/handle-next");                           // partial: 70 of 200
    CHECK(r.status == 200 && r.body.at("delivered") == 70 && r.state().at("stock").at("food") == 0);
    CHECK(r.message().find("(residual)") != std::string::npos);
    CHECK(r.state().at("active").size() == 1 && r.state().at("active")[0].at("quantity") == 130);
    CHECK(r.state().at("log")[2].at("quantityDelivered") == 70 && r.state().at("log")[2].at("quantityRequested") == 200);

    r = call(api, "POST", "/api/handle-next");
    CHECK(r.status == 409 && r.error() == "OutOfStock" && r.message() == "Out of stock for food");
    CHECK(r.state().at("active").size() == 1 && r.state().at("active")[0].at("quantity") == 130);
}

// ---- 6. scenario 4: the AVL tree, rotations, search path --------------------------------------

static void testScenario4Avl() {
    Engine e;
    Api api(e, "data", 8080);
    Reply r = loadScenario(api, "scenario4_avl");
    CHECK(r.state().at("trace") ==
          "Scenario loaded: 6 camps, 6 roads, 0 requests queued | AVL rotations: RR at node 10, RR at node 30, RL at node 20");
    const json& avl = r.state().at("avl");
    CHECK(avl.at("rootId") == 30 && avl.at("height") == 3 && avl.at("nodes").size() == 6);
    // preorder: 30, 20, 10, 25, 40, 50
    CHECK(sameInts(ids(avl.at("nodes"), "id"), {30, 20, 10, 25, 40, 50}));
    CHECK(avl.at("nodes")[0].at("side") == "T" && avl.at("nodes")[1].at("side") == "L" && avl.at("nodes")[4].at("side") == "R");
    CHECK(avl.at("nodes")[0].at("leftId") == 20 && avl.at("nodes")[0].at("rightId") == 40);
    CHECK(avl.at("nodes")[0].at("height") == 3 && avl.at("nodes")[0].at("balance") == 0);   // balance = left - right: 2 - 2
    CHECK(avl.at("nodes")[4].at("balance") == -1 && avl.at("nodes")[4].at("rightId") == 50 && avl.at("nodes")[4].at("leftId") == 0);
    for (const json& n : avl.at("nodes")) CHECK(n.at("balance") >= -1 && n.at("balance") <= 1);
    CHECK(avl.at("rotations").size() == 1 && avl.at("rotations")[0].at("type") == "RL" && avl.at("rotations")[0].at("nodeId") == 20);

    // search: the path the AVL search walks from the root
    r = call(api, "GET", "/api/camps/25");
    CHECK(r.status == 200 && r.ok() && r.body.at("searchPath") == json::array({30, 20, 25}));
    CHECK(r.body.at("camp").at("name") == "Camp25" && r.body.at("route").at("reachable") == true);
    CHECK(!r.body.contains("state"));
    r = call(api, "GET", "/api/camps/30");
    CHECK(r.body.at("searchPath") == json::array({30}));
    r = call(api, "GET", "/api/camps/50");
    CHECK(r.body.at("searchPath") == json::array({30, 40, 50}));
    r = call(api, "GET", "/api/camps/26");                                // not there
    CHECK(r.status == 404 && !r.ok() && r.error() == "NotFound");
    r = call(api, "GET", "/api/camps/0");                                 // the depot is not a camp
    CHECK(r.status == 404);

    // the same tree built through POST /api/camps names each rotation in the trace
    Engine e2;
    Api api2(e2, "data", 8080);
    addCamp(api2, 10, 3, 4);
    addCamp(api2, 20, 6, 8);
    r = call(api2, "POST", "/api/camps", R"({"id":30,"name":"C30","x":9,"y":12,"families":10})");
    CHECK(r.state().at("trace") == "AVL insert 30 caused RR rotation at node 10 | Dijkstra rerun from depot");
    CHECK(r.state().at("avl").at("rotations")[0].at("type") == "RR");
    CHECK(r.state().at("avl").at("rootId") == 20 && r.state().at("avl").at("height") == 2);
}

// ---- 7. camps: add, edit, delete, validation --------------------------------------------------

static void testCamps() {
    Engine e;
    Api api(e, "data", 8080);

    Reply r = call(api, "POST", "/api/camps", R"({"id":1,"name":"  Alpha  ","x":3,"y":4,"families":50,"links":[0]})");
    CHECK(r.status == 200 && r.ok() && r.body.at("roadsAdded") == 1);
    CHECK(r.state().at("camps")[0].at("name") == "Alpha");              // trimmed
    CHECK(r.state().at("roads").size() == 1 && r.state().at("roads")[0].at("distanceKm") == 5.0);
    CHECK(r.state().at("roads")[0].at("minutes") == 8);                 // 5 km at 40 km/h = 7.5, rounds to 8
    CHECK(r.state().at("routes")[0].at("minutes") == 8);

    // duplicate id
    r = call(api, "POST", "/api/camps", R"({"id":1,"name":"Again","x":9,"y":9,"families":5})");
    CHECK(r.status == 409 && r.error() == "Duplicate" && r.message() == "Camp id already exists");
    CHECK(r.state().at("camps").size() == 1);

    // engine-side validation
    r = call(api, "POST", "/api/camps", R"({"id":0,"name":"Depot","x":1,"y":1,"families":5})");
    CHECK(r.status == 400 && r.error() == "Invalid");
    r = call(api, "POST", "/api/camps", R"({"id":2,"name":"Neg","x":1,"y":1,"families":-1})");
    CHECK(r.status == 400 && r.error() == "Invalid");
    r = call(api, "POST", "/api/camps", R"({"id":2,"name":"Far","x":2000000,"y":1,"families":5})");
    CHECK(r.status == 400 && r.error() == "Invalid");
    r = call(api, "POST", "/api/camps", R"({"id":2,"name":"Huge","x":1e999,"y":1,"families":5})");
    CHECK(r.status == 400);
    r = call(api, "POST", "/api/camps", R"({"id":2,"name":"X","x":3,"y":4,"families":5,"links":[1]})");   // same spot as camp 1
    CHECK(r.status == 400 && r.error() == "Invalid" && r.message() == "Link distance would be 0");
    r = call(api, "POST", "/api/camps", R"({"id":2,"name":"X","x":8,"y":8,"families":5,"links":[77]})");
    CHECK(r.status == 404 && r.error() == "NotFound");
    r = call(api, "POST", "/api/camps", R"({"id":2,"name":"X","x":8,"y":8,"families":5,"links":[1,1]})");
    CHECK(r.status == 400 && r.error() == "Invalid");
    r = call(api, "POST", "/api/camps", R"({"id":2,"name":"X","x":8,"y":8,"families":5,"links":[2]})");
    CHECK(r.status == 400 && r.error() == "Invalid");
    CHECK(r.state().at("camps").size() == 1);                            // none of those left a half-added camp
    CHECK(r.state().at("roads").size() == 1);

    // shape and type checks, all 400 BadRequest, nothing changes
    struct Bad { const char* body; };
    const Bad bads[] = {
        {""}, {"not json"}, {"[1,2]"}, {"42"}, {"null"}, {R"({"id":5)"},
        {R"({"name":"A","x":1,"y":1,"families":1})"},                                  // id missing
        {R"({"id":"5","name":"A","x":1,"y":1,"families":1})"},                         // id is text
        {R"({"id":5.5,"name":"A","x":1,"y":1,"families":1})"},                         // id not whole
        {R"({"id":5.0,"name":"A","x":1,"y":1,"families":1})"},
        {R"({"id":1e2,"name":"A","x":1,"y":1,"families":1})"},
        {R"({"id":true,"name":"A","x":1,"y":1,"families":1})"},
        {R"({"id":null,"name":"A","x":1,"y":1,"families":1})"},
        {R"({"id":99999999999,"name":"A","x":1,"y":1,"families":1})"},                 // too big for int
        {R"({"id":-99999999999,"name":"A","x":1,"y":1,"families":1})"},
        {R"({"id":18446744073709551615,"name":"A","x":1,"y":1,"families":1})"},        // bigger than int64
        {R"({"id":2147483648,"name":"A","x":1,"y":1,"families":1})"},                  // INT_MAX + 1
        {R"({"id":5,"name":"","x":1,"y":1,"families":1})"},
        {R"({"id":5,"name":"    ","x":1,"y":1,"families":1})"},
        {R"({"id":5,"name":7,"x":1,"y":1,"families":1})"},
        {R"({"id":5,"name":"line\nbreak","x":1,"y":1,"families":1})"},                 // control character
        {R"({"id":5,"name":"nul\u0000byte","x":1,"y":1,"families":1})"},
        {R"({"id":5,"name":"A","x":"1","y":1,"families":1})"},
        {R"({"id":5,"name":"A","x":null,"y":1,"families":1})"},
        {R"({"id":5,"name":"A","x":1,"y":1})"},                                        // families missing
        {R"({"id":5,"name":"A","x":1,"y":1,"families":1,"extra":1})"},                 // unknown field
        {R"({"id":5,"name":"A","x":1,"y":1,"families":1,"links":"0"})"},
        {R"({"id":5,"name":"A","x":1,"y":1,"families":1,"links":[0,"1"]})"},
        {R"({"id":5,"name":"A","x":1,"y":1,"families":1,"links":[0.5]})"},
        {R"({"id":5,"name":"A","x":1,"y":1,"families":1,"links":null})"},
        {R"({"id":5,"name":"A","x":1,"y":1,"families":1,"links":{"a":1}})"},
    };
    int badCount = 0;
    for (const Bad& b : bads) {
        r = call(api, "POST", "/api/camps", b.body);
        CHECK(r.status == 400 && !r.ok() && r.error() == "BadRequest");
        CHECK(r.state().at("camps").size() == 1);
        badCount++;
    }
    CHECK(badCount > 25);

    // name length: 40 is the longest allowed, 41 is refused
    r = call(api, "POST", "/api/camps", json({{"id", 5}, {"name", std::string(41, 'A')}, {"x", 1}, {"y", 1}, {"families", 1}}).dump());
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "POST", "/api/camps", json({{"id", 5}, {"name", std::string(40, 'A')}, {"x", 1}, {"y", 1}, {"families", 1}}).dump());
    CHECK(r.status == 200 && r.ok());
    r = call(api, "DELETE", "/api/camps/5");
    CHECK(r.status == 200);

    // too many links
    std::string many = R"({"id":5,"name":"A","x":1,"y":1,"families":1,"links":[)";
    for (int i = 0; i < 201; ++i) many += (i ? ",1" : "1");
    many += "]}";
    r = call(api, "POST", "/api/camps", many);
    CHECK(r.status == 400 && r.error() == "BadRequest");

    // the largest legal id is accepted
    r = call(api, "POST", "/api/camps", R"({"id":2147483647,"name":"Max","x":8,"y":8,"families":0})");
    CHECK(r.status == 200 && r.ok());

    // edit
    r = call(api, "PUT", "/api/camps/1", R"({"name":"Alpha2","x":6,"y":8,"families":60})");
    CHECK(r.status == 200 && r.ok());
    CHECK(r.state().at("camps")[0].at("name") == "Alpha2" && r.state().at("camps")[0].at("families") == 60);
    CHECK(r.state().at("roads")[0].at("distanceKm") == 10.0);           // the road followed the camp
    CHECK(r.state().at("trace") == "AVL update 1: key unchanged, no rotation | Dijkstra rerun from depot");
    r = call(api, "PUT", "/api/camps/99", R"({"name":"N","x":1,"y":1,"families":1})");
    CHECK(r.status == 404 && r.error() == "NotFound");
    r = call(api, "PUT", "/api/camps/1", R"({"name":"N","x":1e9,"y":1,"families":1})");
    CHECK(r.status == 400 && r.error() == "Invalid");
    r = call(api, "PUT", "/api/camps/1", R"({"id":1,"name":"N","x":1,"y":1,"families":1})");   // the key is never edited
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "PUT", "/api/camps/1", R"({"name":"N","x":1,"y":1})");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "PUT", "/api/camps/abc", R"({"name":"N","x":1,"y":1,"families":1})");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "GET", "/api/camps/1");
    CHECK(r.body.at("camp").at("name") == "Alpha2");                    // none of the failed edits changed it
}

// ---- 8. delete with confirmation ---------------------------------------------------------------

static void testDeleteCamp() {
    Engine e;
    Api api(e, "data", 8080);
    addCamp(api, 1, 3, 4, "[0]");
    addCamp(api, 2, 6, 8, "[1]");
    call(api, "POST", "/api/requests", R"({"campId":1,"resource":"food","quantity":10,"urgency":2,"families":5})");
    call(api, "POST", "/api/requests", R"({"campId":1,"resource":"water","quantity":10,"urgency":3,"families":5})");
    call(api, "POST", "/api/process");
    call(api, "POST", "/api/requests", R"({"campId":1,"resource":"water","quantity":10,"urgency":3,"families":5})");   // one still queued
    Reply r = call(api, "GET", "/api/camps/1");
    CHECK(r.body.at("camp").at("pending") == 3);

    // 3 pending (2 active + 1 queued): refused until confirmed
    r = call(api, "DELETE", "/api/camps/1");
    CHECK(r.status == 409 && r.error() == "ConfirmNeeded" && r.body.at("needsConfirm") == true && r.body.at("pending") == 3);
    CHECK(r.state().at("camps").size() == 2);
    r = call(api, "DELETE", "/api/camps/1?confirm=false");
    CHECK(r.status == 409 && r.error() == "ConfirmNeeded");
    r = call(api, "DELETE", "/api/camps/1", R"({"confirm":false})");
    CHECK(r.status == 409 && r.error() == "ConfirmNeeded");
    r = call(api, "DELETE", "/api/camps/1?confirm=maybe");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "DELETE", "/api/camps/1?force=true");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "DELETE", "/api/camps/1", R"({"confirm":"yes"})");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "DELETE", "/api/camps/1", "not json");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    CHECK(call(api, "GET", "/api/state").state().at("camps").size() == 2);   // still there after every refusal

    r = call(api, "DELETE", "/api/camps/1?confirm=true");
    CHECK(r.status == 200 && r.ok() && r.body.at("cancelledRequests") == 3);
    CHECK(r.state().at("camps").size() == 1 && r.state().at("queue").empty() && r.state().at("active").empty());
    CHECK(r.state().at("roads").empty());                               // its roads went with it
    CHECK(r.state().at("trace").get<std::string>().find("AVL delete 1") == 0);
    CHECK(r.state().at("routes")[0].at("reachable") == false);          // camp 2 lost its only link

    // the body form of confirm
    addCamp(api, 3, 1, 1, "[0]");
    call(api, "POST", "/api/requests", R"({"campId":3,"resource":"food","quantity":1,"urgency":1,"families":1})");
    r = call(api, "DELETE", "/api/camps/3", R"({"confirm":true})");
    CHECK(r.status == 200 && r.body.at("cancelledRequests") == 1);

    // no pending requests: no confirmation needed
    r = call(api, "DELETE", "/api/camps/2");
    CHECK(r.status == 200 && r.body.at("cancelledRequests") == 0 && r.state().at("camps").empty());

    r = call(api, "DELETE", "/api/camps/2");
    CHECK(r.status == 404 && r.error() == "NotFound");
    r = call(api, "DELETE", "/api/camps/0");
    CHECK(r.status == 400 && r.error() == "Invalid" && r.message() == "Cannot delete depot");
    r = call(api, "DELETE", "/api/camps/-5");
    CHECK(r.status == 400 && r.error() == "Invalid" && r.message() == "Invalid camp id");
    r = call(api, "DELETE", "/api/camps/12x");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "DELETE", "/api/camps/99999999999");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "DELETE", "/api/camps/");
    CHECK(r.status == 400 && r.error() == "BadRequest");
}

// ---- 9. roads ----------------------------------------------------------------------------------

static void testRoads() {
    Engine e;
    Api api(e, "data", 8080);
    addCamp(api, 1, 3, 4, "[0]");
    addCamp(api, 2, 6, 8, "[0]");

    Reply r = call(api, "POST", "/api/roads", R"({"a":1,"b":2})");
    CHECK(r.status == 200 && r.ok() && r.state().at("roads").size() == 3);
    CHECK(r.state().at("trace") == "Road added between 1 and 2 | Dijkstra rerun from depot");
    r = call(api, "POST", "/api/roads", R"({"a":2,"b":1})");                         // same road, other order
    CHECK(r.status == 409 && r.error() == "Duplicate" && r.message() == "Road already exists");
    r = call(api, "POST", "/api/roads", R"({"a":1,"b":1})");
    CHECK(r.status == 400 && r.error() == "Invalid");
    r = call(api, "POST", "/api/roads", R"({"a":1,"b":9})");
    CHECK(r.status == 404 && r.error() == "NotFound");
    r = call(api, "POST", "/api/roads", R"({"a":1})");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "POST", "/api/roads", R"({"a":1,"b":"2"})");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "POST", "/api/roads", R"({"a":1,"b":2,"status":"open"})");         // wrong endpoint for status
    CHECK(r.status == 400 && r.error() == "BadRequest");
    CHECK(call(api, "GET", "/api/state").state().at("roads").size() == 3);

    const char* badStatus[] = {R"({"a":1,"b":2,"status":"closed"})", R"({"a":1,"b":2,"status":"OPEN"})",
                               R"({"a":1,"b":2,"status":1})", R"({"a":1,"b":2})", R"({"a":1,"b":2,"status":null})"};
    for (const char* body : badStatus) {
        r = call(api, "PUT", "/api/roads", body);
        CHECK(r.status == 400 && r.error() == "BadRequest");
    }
    r = call(api, "PUT", "/api/roads", R"({"a":1,"b":2,"status":"slow"})");
    CHECK(r.status == 200 && r.state().at("trace") == "Road 1-2 set to slow | Dijkstra rerun from depot");
    r = call(api, "PUT", "/api/roads", R"({"a":1,"b":7,"status":"slow"})");
    CHECK(r.status == 404 && r.error() == "NotFound" && r.message() == "Road not found");

    r = call(api, "DELETE", "/api/roads", R"({"a":2,"b":1})");
    CHECK(r.status == 200 && r.state().at("roads").size() == 2);
    r = call(api, "DELETE", "/api/roads", R"({"a":2,"b":1})");
    CHECK(r.status == 404 && r.error() == "NotFound");
    r = call(api, "DELETE", "/api/roads", "");
    CHECK(r.status == 400 && r.error() == "BadRequest");
}

// ---- 10. requests: submit, process, update -----------------------------------------------------

static void testRequests() {
    Engine e;
    Api api(e, "data", 8080);
    addCamp(api, 1, 3, 4, "[0]");

    Reply r = call(api, "POST", "/api/requests", R"({"campId":1,"resource":"food","quantity":50,"urgency":2,"families":30})");
    CHECK(r.status == 200 && r.ok() && r.body.at("requestId") == 1);
    CHECK(r.state().at("queue").size() == 1 && r.state().at("active").empty());
    CHECK(r.state().at("trace") == "Queue enqueue: request 1 for camp 1");
    CHECK(r.state().at("queue")[0].at("arrivalSeq") == 1 && r.state().at("queue")[0].at("resource") == "food");

    // each of these must leave the id counter alone, so the next good request is still 2
    const char* bads[] = {
        R"({"campId":1,"resource":"ice","quantity":5,"urgency":2,"families":3})",
        R"({"campId":1,"resource":"Food","quantity":5,"urgency":2,"families":3})",
        R"({"campId":1,"resource":2,"quantity":5,"urgency":2,"families":3})",
        R"({"campId":1,"resource":"food","quantity":"5","urgency":2,"families":3})",
        R"({"campId":1,"resource":"food","quantity":5.5,"urgency":2,"families":3})",
        R"({"campId":1,"resource":"food","quantity":5,"urgency":2})",
        R"({"campId":1,"resource":"food","quantity":5,"urgency":2,"families":3,"note":"x"})",
        R"({"campId":1,"resource":"food","quantity":99999999999,"urgency":2,"families":3})",
    };
    for (const char* body : bads) {
        r = call(api, "POST", "/api/requests", body);
        CHECK(r.status == 400 && r.error() == "BadRequest");
    }
    const char* invalid[] = {
        R"({"campId":1,"resource":"food","quantity":0,"urgency":2,"families":3})",
        R"({"campId":1,"resource":"food","quantity":-5,"urgency":2,"families":3})",
        R"({"campId":1,"resource":"food","quantity":5,"urgency":0,"families":3})",
        R"({"campId":1,"resource":"food","quantity":5,"urgency":5,"families":3})",
        R"({"campId":1,"resource":"food","quantity":5,"urgency":2,"families":0})",
        R"({"campId":0,"resource":"food","quantity":5,"urgency":2,"families":3})",
    };
    for (const char* body : invalid) {
        r = call(api, "POST", "/api/requests", body);
        CHECK(r.status == 400 && r.error() == "Invalid");
    }
    r = call(api, "POST", "/api/requests", R"({"campId":42,"resource":"food","quantity":5,"urgency":2,"families":3})");
    CHECK(r.status == 404 && r.error() == "NotFound" && r.message() == "Camp not found");
    CHECK(r.state().at("queue").size() == 1);

    r = call(api, "POST", "/api/requests", R"({"campId":1,"resource":"water","quantity":10,"urgency":4,"families":5})");
    CHECK(r.status == 200 && r.body.at("requestId") == 2);

    // update before processing: the request is still in the queue, not the active list
    r = call(api, "PUT", "/api/requests/1", R"({"urgency":3,"families":30,"quantity":50})");
    CHECK(r.status == 404 && r.error() == "NotFound");

    r = call(api, "POST", "/api/process");
    CHECK(r.status == 200 && r.body.at("moved") == 2 && r.state().at("queue").empty());
    CHECK(sameInts(ids(r.state().at("active"), "id"), {2, 1}));
    CHECK(r.state().at("trace") == "Process incoming: 2 moved to the active list, merge sort run");
    r = call(api, "POST", "/api/process");
    CHECK(r.status == 409 && r.error() == "Empty" && r.message() == "Queue is empty");

    // raising request 1 to critical with more families moves it to the top
    r = call(api, "PUT", "/api/requests/1", R"({"urgency":4,"families":99,"quantity":50})");
    CHECK(r.status == 200 && r.ok());
    CHECK(sameInts(ids(r.state().at("active"), "id"), {1, 2}));
    CHECK(r.state().at("active")[0].at("reason") == "families" && r.state().at("active")[0].at("urgency") == 4);
    CHECK(r.state().at("trace") == "Update request: 1 updated, merge sort run");

    r = call(api, "PUT", "/api/requests/1", R"({"urgency":9,"families":1,"quantity":1})");
    CHECK(r.status == 400 && r.error() == "Invalid");
    r = call(api, "PUT", "/api/requests/77", R"({"urgency":2,"families":1,"quantity":1})");
    CHECK(r.status == 404 && r.error() == "NotFound");
    r = call(api, "PUT", "/api/requests/1", R"({"urgency":2,"families":1})");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "PUT", "/api/requests/x", R"({"urgency":2,"families":1,"quantity":1})");
    CHECK(r.status == 400 && r.error() == "BadRequest");
    r = call(api, "PUT", "/api/requests/1", R"({"urgency":2,"families":1,"quantity":1,"campId":2})");   // the camp is never edited
    CHECK(r.status == 400 && r.error() == "BadRequest");
    CHECK(call(api, "GET", "/api/state").state().at("active")[0].at("families") == 99);
}

// ---- 11. settings, reset, scenarios ------------------------------------------------------------

static void testSettingsResetScenarios() {
    Engine e;
    Api api(e, "data", 8080);
    loadScenario(api, "scenario1_normal");

    Reply r = call(api, "POST", "/api/settings", R"({"speedKmh":30})");
    CHECK(r.status == 200 && r.ok() && r.state().at("speedKmh") == 30.0);
    CHECK(r.state().at("routes")[0].at("minutes") == 10);               // 5 km at 30 km/h
    CHECK(r.state().at("roads")[0].at("minutes") == 10);

    const char* bads[] = {R"({"speedKmh":0})", R"({"speedKmh":-5})", R"({"speedKmh":"fast"})", R"({})",
                          R"({"speed":50})", R"({"speedKmh":null})", R"({"speedKmh":50,"stock":1})", "[]", ""};
    for (const char* body : bads) {
        r = call(api, "POST", "/api/settings", body);
        CHECK(r.status == 400 && (r.error() == "BadRequest" || r.error() == "Invalid"));
        CHECK(r.state().at("speedKmh") == 30.0);                         // refused: speed unchanged
    }
    r = call(api, "POST", "/api/settings", R"({"speedKmh":1e-300})");    // legal but absurd: minutes clamp, nothing overflows
    CHECK(r.status == 200 || r.status == 400);
    r = call(api, "POST", "/api/settings", R"({"speedKmh":60})");
    CHECK(r.status == 200 && r.state().at("routes")[5].at("minutes") == 16);

    // every scenario loads and gives the right sizes
    const char* names[4] = {"scenario1_normal", "scenario2_blockage", "scenario3_priority", "scenario4_avl"};
    size_t camps[4]  = {6, 6, 6, 6};
    size_t queued[4] = {6, 1, 3, 0};
    for (int i = 0; i < 4; ++i) {
        r = loadScenario(api, names[i]);
        CHECK(r.status == 200 && r.ok() && r.state().at("camps").size() == camps[i] && r.state().at("queue").size() == queued[i]);
    }

    // unknown names, and names that try to leave the data folder
    const char* badNames[] = {"nope", "scenario1_normal.txt", "scenario1_normal.expected", "../README", "..%2FREADME",
                              "scenario1_normal/../scenario2_blockage", "SCENARIO1_NORMAL", "", "scenario"};
    for (const char* n : badNames) {
        r = call(api, "POST", std::string("/api/scenario/") + n);
        CHECK((r.status == 404 || r.status == 400) && !r.ok());
    }
    CHECK(call(api, "GET", "/api/state").state().at("camps").size() == 6);   // scenario 4 is still loaded

    // reset
    r = call(api, "POST", "/api/reset");
    CHECK(r.status == 200 && r.ok() && r.state().at("camps").empty() && r.state().at("trace") == "Reset.");
    CHECK(r.state().at("speedKmh") == 40.0 && r.state().at("stock").at("food") == 500);
}

// A missing data folder must not wipe a session; a broken file reports its line and resets.
static void testScenarioFileProblems() {
    Engine e;
    Api good(e, "data", 8080);
    loadScenario(good, "scenario1_normal");

    Api broken(e, "no_such_folder", 8080);
    Reply r = loadScenario(broken, "scenario1_normal");
    CHECK(r.status == 500 && r.error() == "ServerError");
    CHECK(r.message().find("Cannot read no_such_folder/scenario1_normal.txt") == 0);
    CHECK(r.state().at("camps").size() == 6 && r.state().at("queue").size() == 6);   // nothing was wiped

    // a scenario file with a bad line: the Engine resets itself (spec) and the message names the line
    const char* path = "tests/server/scenario1_normal.txt";
    std::FILE* f = std::fopen(path, "wb");
    CHECK(f != nullptr);
    if (f) {
        std::fputs("depot 0 0\nbogus line\n", f);
        std::fclose(f);
        Api bad(e, "tests/server", 8080);
        r = loadScenario(bad, "scenario1_normal");
        CHECK(r.status == 400 && r.error() == "Invalid" && r.message() == "line 2: unknown command");
        CHECK(r.state().at("camps").empty());                            // the state shown is the real, emptied one
        std::remove(path);
    }

    // a camp name that is not valid UTF-8 (from a hand-edited file) must not break the snapshot
    Engine e2;
    Api api2(e2, "data", 8080);
    e2.loadScenario("camp 1 Bad\xff\xfeName 3 4 10\n");
    r = call(api2, "GET", "/api/state");
    CHECK(r.status == 200 && r.ok() && r.state().at("camps").size() == 1);
}

// ---- 12. routing, methods, origin, hostile input -----------------------------------------------

static void testRoutingAndSafety() {
    Engine e;
    Api api(e, "data", 8080);

    Reply r = call(api, "GET", "/api/nothing");
    CHECK(r.status == 404 && r.error() == "NotFound" && !r.body.contains("state"));
    CHECK(call(api, "GET", "/api/").status == 404);
    CHECK(call(api, "GET", "/api").status == 404);
    CHECK(call(api, "GET", "/api/state/").status == 404);
    CHECK(call(api, "GET", "/api/state/extra").status == 404);
    CHECK(call(api, "GET", "/other").status == 404);
    CHECK(call(api, "GET", "/api/camps/1/2").status == 404);
    CHECK(call(api, "GET", "/api/process/now").status == 404);

    // wrong method on a real path: 405
    const char* wrong[][2] = {
        {"POST", "/api/state"}, {"DELETE", "/api/state"}, {"GET", "/api/process"}, {"GET", "/api/handle-next"},
        {"PUT", "/api/process"}, {"GET", "/api/reset"}, {"GET", "/api/settings"}, {"GET", "/api/scenario/scenario1_normal"},
        {"GET", "/api/camps"}, {"PUT", "/api/camps"}, {"POST", "/api/camps/1"}, {"GET", "/api/roads"},
        {"PATCH", "/api/roads"}, {"GET", "/api/requests"}, {"POST", "/api/requests/1"}, {"DELETE", "/api/requests/1"},
        {"PATCH", "/api/state"}, {"OPTIONS", "/api/state"},
    };
    for (const auto& w : wrong) {
        r = call(api, w[0], w[1]);
        CHECK(r.status == 405 && r.error() == "MethodNotAllowed");
    }
    CHECK(call(api, "GET", "/api/state").state().at("camps").empty());     // none of that changed anything

    // Origin: a foreign page may not change state; the server's own page may; curl sends none.
    std::string camp = R"({"id":1,"name":"A","x":3,"y":4,"families":5})";
    r = call(api, "POST", "/api/camps", camp, "http://evil.example");
    CHECK(r.status == 403 && r.error() == "Forbidden");
    CHECK(call(api, "POST", "/api/camps", camp, "http://localhost:9999").status == 403);     // other port
    CHECK(call(api, "POST", "/api/camps", camp, "http://localhost:8080.evil.example").status == 403);
    CHECK(call(api, "POST", "/api/camps", camp, "https://localhost:8080").status == 403);
    CHECK(call(api, "POST", "/api/camps", camp, "null").status == 403);
    CHECK(call(api, "POST", "/api/reset", "", "http://evil.example").status == 403);
    CHECK(call(api, "DELETE", "/api/camps/1", "", "http://evil.example").status == 403);
    CHECK(call(api, "GET", "/api/state").state().at("camps").empty());
    CHECK(call(api, "POST", "/api/camps", camp, "http://localhost:8080").status == 200);
    CHECK(call(api, "POST", "/api/process", "", "http://127.0.0.1:8080").status == 409);   // allowed through, then Empty
    CHECK(call(api, "POST", "/api/process", "", "").status == 409);

    // hostile bodies: nothing may crash or leak (the sanitizer build is the judge of that)
    CHECK(call(api, "POST", "/api/camps", std::string(60000, '[')).status == 400);               // 60,000 open brackets
    CHECK(call(api, "POST", "/api/camps", std::string(30000, '[') + std::string(30000, ']')).status == 400);
    CHECK(call(api, "POST", "/api/camps", std::string("{\"id\":") + std::string(5000, '9') + "}").status == 400);
    CHECK(call(api, "POST", "/api/camps", std::string("\xff\xfe\x00garbage", 10)).status == 400);
    std::string longKey = "{\"" + std::string(50000, 'k') + "\":1}";
    r = call(api, "POST", "/api/camps", longKey);
    CHECK(r.status == 400 && r.message().size() < 100);                                          // the key is not echoed whole
    r = call(api, "POST", "/api/camps", R"({"id":2,"name":"<script>alert(1)</script>","x":50,"y":50,"families":1})");
    CHECK(r.status == 200);                                                                      // stored as plain text
    CHECK(r.state().at("camps")[1].at("name") == "<script>alert(1)</script>");
    CHECK(r.text.find("<script>") != std::string::npos);                                         // JSON-escaping is the browser's job (textContent)
    CHECK(r.status == 200 && r.state().at("camps").size() == 2);
}

// ---- 13. the API must not change what the Engine does -------------------------------------------
// The same random operations go through the API and, directly, to a second Engine. After every
// operation the answer and the whole visible state must be identical.

static unsigned g_seed = 1;
static int rnd(int n) {                               // small deterministic generator, same on every platform
    g_seed = g_seed * 1103515245u + 12345u;
    return static_cast<int>((g_seed >> 16) % static_cast<unsigned>(n));
}

static void sameState(const json& s, const Engine& ref) {
    CHECK(s.at("trace") == ref.lastTrace());
    CHECK(s.at("speedKmh") == ref.averageSpeed());
    Stock st = ref.stock();
    CHECK(s.at("stock").at("food") == st.food && s.at("stock").at("water") == st.water && s.at("stock").at("medicine") == st.medicine);
    std::vector<Camp> camps = ref.camps();
    CHECK(s.at("camps").size() == camps.size());
    for (std::size_t i = 0; i < camps.size() && i < s.at("camps").size(); ++i) {
        const json& c = s.at("camps")[i];
        CHECK(c.at("id") == camps[i].id && c.at("name") == camps[i].name);
        CHECK(c.at("x") == camps[i].x && c.at("y") == camps[i].y && c.at("families") == camps[i].families);
        CHECK(c.at("received").at("food") == camps[i].receivedFood && c.at("received").at("water") == camps[i].receivedWater);
        CHECK(c.at("received").at("medicine") == camps[i].receivedMedicine);
        CHECK(c.at("pending") == ref.pendingCountForCamp(camps[i].id));
    }
    std::vector<EdgeInfo> roads = ref.roads();
    CHECK(s.at("roads").size() == roads.size());
    for (std::size_t i = 0; i < roads.size() && i < s.at("roads").size(); ++i) {
        CHECK(s.at("roads")[i].at("a") == roads[i].a && s.at("roads")[i].at("b") == roads[i].b);
        CHECK(s.at("roads")[i].at("minutes") == roads[i].minutes && s.at("roads")[i].at("distanceKm") == roads[i].distanceKm);
    }
    std::vector<RouteInfo> routes = ref.routes();
    CHECK(s.at("routes").size() == routes.size());
    for (std::size_t i = 0; i < routes.size() && i < s.at("routes").size(); ++i) {
        CHECK(s.at("routes")[i].at("reachable") == routes[i].reachable);
        CHECK(s.at("routes")[i].at("path") == json(routes[i].path));
        if (routes[i].reachable) CHECK(s.at("routes")[i].at("minutes") == routes[i].minutes);
    }
    std::vector<Request> q = ref.queueContents();
    CHECK(s.at("queue").size() == q.size());
    for (std::size_t i = 0; i < q.size() && i < s.at("queue").size(); ++i) {
        CHECK(s.at("queue")[i].at("id") == q[i].id && s.at("queue")[i].at("arrivalSeq") == q[i].arrivalSeq);
    }
    std::vector<RankedRequest> act = ref.rankedRequests();
    CHECK(s.at("active").size() == act.size());
    for (std::size_t i = 0; i < act.size() && i < s.at("active").size(); ++i) {
        CHECK(s.at("active")[i].at("id") == act[i].req.id && s.at("active")[i].at("quantity") == act[i].req.quantity);
        if (act[i].hasReason) CHECK(s.at("active")[i].at("reason") == rankKeyName(act[i].reason));
        else CHECK(s.at("active")[i].at("reason").is_null());
    }
    std::vector<DeliveryRecord> log = ref.deliveries();
    CHECK(s.at("log").size() == log.size());
    for (std::size_t i = 0; i < log.size() && i < s.at("log").size(); ++i) {
        CHECK(s.at("log")[i].at("requestId") == log[i].requestId && s.at("log")[i].at("quantityDelivered") == log[i].quantityDelivered);
        CHECK(s.at("log")[i].at("totalMinutes") == log[i].totalMinutes && s.at("log")[i].at("path") == json(log[i].path));
    }
    std::vector<AVLNodeInfo> nodes;
    ref.avlSnapshot(nodes);
    CHECK(s.at("avl").at("nodes").size() == nodes.size());
    for (std::size_t i = 0; i < nodes.size() && i < s.at("avl").at("nodes").size(); ++i) {
        const json& n = s.at("avl").at("nodes")[i];
        CHECK(n.at("id") == nodes[i].id && n.at("height") == nodes[i].height && n.at("balance") == nodes[i].balance);
        CHECK(n.at("leftId") == nodes[i].leftId && n.at("rightId") == nodes[i].rightId && n.at("depth") == nodes[i].depth);
    }
    CHECK(s.at("dijkstra").at("steps").size() == ref.dijkstra().steps().size());
}

static void testParityWithEngine(unsigned seed, int operations) {
    g_seed = seed;
    Engine viaApi;
    Engine direct;
    Api api(viaApi, "data", 8080);

    for (int step = 0; step < operations; ++step) {
        int op = rnd(15);
        Reply r;
        OpResult want{Err::None, "", 0};
        bool compareResult = true;

        if (op <= 2) {                                                    // add camp
            int id = 1 + rnd(12);
            double x = rnd(40) - 5 + (rnd(2) ? 0.5 : 0.0), y = rnd(40) - 5;
            std::vector<int> links;
            for (int k = rnd(4); k > 0; --k) links.push_back(rnd(13));    // 0..12, may repeat, may not exist
            json body = {{"id", id}, {"name", "N" + std::to_string(id)}, {"x", x}, {"y", y}, {"families", rnd(100)}, {"links", links}};
            r = call(api, "POST", "/api/camps", body.dump());
            Camp c; c.id = id; c.name = "N" + std::to_string(id); c.x = x; c.y = y; c.families = body.at("families");
            c.receivedFood = c.receivedWater = c.receivedMedicine = 0;
            want = direct.addCamp(c, links);
        } else if (op == 3) {                                             // edit camp
            int id = 1 + rnd(12);
            double x = rnd(40), y = rnd(40);
            int fam = rnd(100);
            json body = {{"name", "E" + std::to_string(id)}, {"x", x}, {"y", y}, {"families", fam}};
            r = call(api, "PUT", "/api/camps/" + std::to_string(id), body.dump());
            want = direct.editCamp(id, "E" + std::to_string(id), x, y, fam);
        } else if (op == 4) {                                             // delete camp
            int id = 1 + rnd(12);
            bool confirm = rnd(2) == 1;
            if (rnd(2)) r = call(api, "DELETE", "/api/camps/" + std::to_string(id) + (confirm ? "?confirm=true" : ""));
            else        r = call(api, "DELETE", "/api/camps/" + std::to_string(id), confirm ? R"({"confirm":true})" : "");
            want = direct.deleteCamp(id, confirm);
        } else if (op == 5) {                                             // add road
            int a = rnd(13), b = rnd(13);
            r = call(api, "POST", "/api/roads", json({{"a", a}, {"b", b}}).dump());
            want = direct.addRoad(a, b);
        } else if (op == 6) {                                             // remove road
            int a = rnd(13), b = rnd(13);
            r = call(api, "DELETE", "/api/roads", json({{"a", a}, {"b", b}}).dump());
            want = direct.removeRoad(a, b);
        } else if (op == 7) {                                             // road status
            int a = rnd(13), b = rnd(13), s = rnd(3);
            const char* names[3] = {"open", "slow", "blocked"};
            RoadStatus st[3] = {RoadStatus::Open, RoadStatus::Slow, RoadStatus::Blocked};
            r = call(api, "PUT", "/api/roads", json({{"a", a}, {"b", b}, {"status", names[s]}}).dump());
            want = direct.setRoadStatus(a, b, st[s]);
        } else if (op == 8 || op == 9) {                                  // submit request
            int camp = 1 + rnd(12), res = rnd(3), qty = 1 + rnd(120), urg = 1 + rnd(4), fam = 1 + rnd(200);
            const char* names[3] = {"food", "water", "medicine"};
            ResourceType rt[3] = {ResourceType::Food, ResourceType::Water, ResourceType::Medicine};
            r = call(api, "POST", "/api/requests",
                     json({{"campId", camp}, {"resource", names[res]}, {"quantity", qty}, {"urgency", urg}, {"families", fam}}).dump());
            want = direct.submitRequest(camp, rt[res], qty, urg, fam);
        } else if (op == 10) {                                            // process
            r = call(api, "POST", "/api/process");
            want = direct.processIncoming();
        } else if (op == 11 || op == 12) {                                // handle next
            r = call(api, "POST", "/api/handle-next");
            want = direct.handleNext();
        } else if (op == 13) {                                            // update request, or speed
            if (rnd(2)) {
                int id = 1 + rnd(10), urg = 1 + rnd(4), fam = 1 + rnd(200), qty = 1 + rnd(120);
                r = call(api, "PUT", "/api/requests/" + std::to_string(id),
                         json({{"urgency", urg}, {"families", fam}, {"quantity", qty}}).dump());
                want = direct.updateRequest(id, urg, fam, qty);
            } else {
                double kmh = 10 + rnd(90);
                r = call(api, "POST", "/api/settings", json({{"speedKmh", kmh}}).dump());
                want = direct.setAverageSpeed(kmh);
            }
        } else {                                                          // scenario or reset (rare)
            if (rnd(4) == 0) {
                r = call(api, "POST", "/api/reset");
                direct.reset();
                compareResult = false;
            } else {
                const char* names[4] = {"scenario1_normal", "scenario2_blockage", "scenario3_priority", "scenario4_avl"};
                int n = rnd(4);
                r = loadScenario(api, names[n]);
                want = direct.loadScenario(readTextFile((std::string("data/") + names[n] + ".txt").c_str()));
            }
        }

        if (compareResult) {
            Err got = Err::None;
            const char* names[8] = {"None", "Invalid", "Duplicate", "NotFound", "ConfirmNeeded", "Empty", "Unreachable", "OutOfStock"};
            for (int k = 0; k < 8; ++k) if (r.error() == names[k] || (k == 0 && r.error().empty())) got = static_cast<Err>(k);
            CHECK(r.error() != "BadRequest");                             // well-typed input must reach the Engine
            CHECK(got == want.err);
            CHECK(r.message() == want.message);
            CHECK(r.ok() == want.ok());
        }
        sameState(r.state(), direct);
    }
}

// ---- main ---------------------------------------------------------------------------------------

// A missing JSON key makes json::at() throw. Count that as a failure and keep going,
// rather than aborting the whole run.
static void run(const char* name, void (*test)()) {
    std::printf("--- Api: %s ---\n", name);
    try {
        test();
    } catch (const std::exception& e) {
        ++g_failures;
        std::printf("  FAIL exception in '%s': %s\n", name, e.what());
    }
}

static void testParity() {
    for (unsigned seed = 1; seed <= 12; ++seed) testParityWithEngine(seed, 250);
}

int main() {
    run("empty state", testEmptyState);
    run("scenario 1", testScenario1);
    run("handle next", testHandleNextScenario1);
    run("scenario 2 (blockage)", testScenario2Blockage);
    run("scenario 3 (priority)", testScenario3Priority);
    run("scenario 4 (AVL)", testScenario4Avl);
    run("camps", testCamps);
    run("delete camp", testDeleteCamp);
    run("roads", testRoads);
    run("requests", testRequests);
    run("settings, reset, scenarios", testSettingsResetScenarios);
    run("scenario file problems", testScenarioFileProblems);
    run("routing and safety", testRoutingAndSafety);
    run("parity with the Engine", testParity);

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures > 0 ? 1 : 0;
}
