// ReliefNet local server (Phase 3).
//
// This file only moves bytes. It owns one Engine, hands every /api request to Api::handle
// (server/Api.cpp, where the endpoints live), and serves the files in web/ for everything else.
//
// Run it from the repository root: the scenario files are read from data/ and the page from web/.
//
//   relief_server            listen on http://localhost:8080
//   relief_server 9000       listen on http://localhost:9000

#include <cstdio>
#include <mutex>
#include <string>

#include "httplib.h"
#include "Api.h"

static const int kDefaultPort = 8080;

// Shown at "/" only while web/index.html does not exist (before Phase 4 is installed).
static const char* kFallbackPage =
    "<!doctype html><html><head><meta charset=\"utf-8\"><title>ReliefNet server</title></head>"
    "<body style=\"font-family:sans-serif;max-width:40em;margin:2em auto\">"
    "<h1>ReliefNet server is running</h1>"
    "<p>The page in <code>web/</code> is not installed yet. The API works:</p>"
    "<ul><li><a href=\"/api/state\">GET /api/state</a></li></ul>"
    "<p>See README.md for every endpoint.</p></body></html>";

static bool readPort(const char* text, int& port) {
    return parseWholeNumber(text, port) && port >= 1 && port <= 65535;
}

static bool fileExists(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

int main(int argc, char** argv) {
    int port = kDefaultPort;
    if (argc > 2 || (argc == 2 && !readPort(argv[1], port))) {
        std::fprintf(stderr, "Usage: %s [port]    (port 1 to 65535, default %d)\n", argv[0], kDefaultPort);
        return 1;
    }

    if (!fileExists("data/scenario1_normal.txt")) {
        std::fprintf(stderr, "Warning: data/scenario1_normal.txt not found. "
                             "Start the server from the repository root, or loading scenarios will fail.\n");
    }

    Engine engine;
    Api api(engine, "data", port);

    // The Engine is not thread safe and httplib answers requests on several threads,
    // so every call into it goes through this one lock.
    std::mutex engineLock;
    std::mutex printLock;

    httplib::Server svr;
    svr.set_payload_max_length(64 * 1024);   // no request body here needs more than a few hundred bytes

    bool haveWeb = svr.set_mount_point("/", "web");
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(kFallbackPage, "text/html; charset=utf-8");   // reached only if web/index.html is missing
    });

    auto handler = [&](const httplib::Request& req, httplib::Response& res) {
        ApiRequest in;
        in.method = req.method;
        in.path   = req.path;
        for (const auto& kv : req.params) in.query.push_back(kv);
        in.body   = req.body;
        in.origin = req.get_header_value("Origin");

        ApiResponse out;
        {
            std::lock_guard<std::mutex> guard(engineLock);
            out = api.handle(in);
        }
        res.status = out.status;
        res.set_content(out.body, "application/json; charset=utf-8");
    };
    svr.Get("/api/.*", handler);
    svr.Post("/api/.*", handler);
    svr.Put("/api/.*", handler);
    svr.Delete("/api/.*", handler);

    // Anything httplib itself refuses (404, 413 too large, ...) still gets a JSON body under /api/.
    // httplib calls this for EVERY 4xx/5xx, including the ones Api::handle built, so it must only
    // fill in a body when there is none.
    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.path.compare(0, 5, "/api/") == 0 && res.body.empty()) {
            res.set_content("{\"ok\":false,\"error\":\"HttpError\",\"message\":\"HTTP " +
                            std::to_string(res.status) + "\"}", "application/json; charset=utf-8");
        }
    });
    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr) {
        res.status = 500;
        res.set_content("{\"ok\":false,\"error\":\"ServerError\",\"message\":\"Internal error\"}",
                        "application/json; charset=utf-8");
    });

    // Never let the browser reuse an old copy of the page or of a response while we are developing.
    svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-store");
        res.set_header("X-Content-Type-Options", "nosniff");
    });

    // One line per API call, so the console shows what the page is asking the engine to do.
    svr.set_logger([&](const httplib::Request& req, const httplib::Response& res) {
        if (req.path.compare(0, 5, "/api/") != 0) return;
        std::lock_guard<std::mutex> guard(printLock);
        std::printf("%-6s %-28s -> %d\n", req.method.c_str(), req.path.c_str(), res.status);
        std::fflush(stdout);
    });

    // Local only: bind to the loopback address, never to the network.
    if (!svr.bind_to_port("127.0.0.1", port)) {
        std::fprintf(stderr, "Cannot listen on port %d (is another program using it?). Try: %s %d\n",
                     port, argv[0], port == kDefaultPort ? 8081 : kDefaultPort);
        return 1;
    }

    std::printf("ReliefNet server: http://localhost:%d/   (Ctrl+C to stop)\n", port);
    if (!haveWeb) {
        std::printf("web/ folder not found: only the API and a placeholder page are served.\n");
    }
    std::fflush(stdout);

    svr.listen_after_bind();
    return 0;
}
