// The API layer of the ReliefNet server (Phase 3).
//
// Api::handle takes one request (method, path, query, body, Origin header) and returns a status
// and a JSON body. It knows nothing about sockets, so the tests call it directly and main.cpp is
// the only file that touches httplib. It reads the Engine only through the Engine's public
// functions: nothing in engine/ was changed for the server.
//
// Not thread safe: the caller must hold one lock around every call to handle (main.cpp does).

#ifndef API_H
#define API_H

#include <string>
#include <utility>
#include <vector>
#include "Engine.h"

struct ApiRequest {
    std::string method;                                        // "GET", "POST", "PUT" or "DELETE"
    std::string path;                                          // "/api/camps/5", no query string
    std::vector<std::pair<std::string, std::string>> query;    // decoded ?name=value pairs, in order
    std::string body;                                          // raw request body (JSON text, or empty)
    std::string origin;                                        // the Origin header, "" if none was sent
};

struct ApiResponse {
    int         status = 500;
    std::string body;                                          // always JSON text
};

class Api {
public:
    // dataDir: the folder holding the scenario files (normally "data"). port: the port the server
    // listens on, used to decide which Origin header counts as "our own page".
    Api(Engine& engine, const std::string& dataDir, int port);

    ApiResponse handle(const ApiRequest& req);

    // The four scenario names the server will load (file names without ".txt").
    static const std::vector<std::string>& scenarioNames();

private:
    Engine&     engine_;
    std::string dataDir_;
    int         port_;
};

#endif // API_H
