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
#include <cstdlib>
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

static std::string envOr(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : std::string(fallback);
}

static std::string base64Encode(const std::string& in) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    while (i + 2 < in.size()) {
        unsigned n = (unsigned char)in[i] << 16 | (unsigned char)in[i + 1] << 8 | (unsigned char)in[i + 2];
        out += tbl[n >> 18 & 63]; out += tbl[n >> 12 & 63]; out += tbl[n >> 6 & 63]; out += tbl[n & 63];
        i += 3;
    }
    if (i + 1 == in.size()) {
        unsigned n = (unsigned char)in[i] << 16;
        out += tbl[n >> 18 & 63]; out += tbl[n >> 12 & 63]; out += "==";
    } else if (i + 2 == in.size()) {
        unsigned n = (unsigned char)in[i] << 16 | (unsigned char)in[i + 1] << 8;
        out += tbl[n >> 18 & 63]; out += tbl[n >> 12 & 63]; out += tbl[n >> 6 & 63]; out += '=';
    }
    return out;
}

// Compare without stopping at the first difference.
static bool sameText(const std::string& a, const std::string& b) {
    unsigned diff = (unsigned)(a.size() ^ b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

int main(int argc, char** argv) {
    int port = kDefaultPort;
    // Hosting platforms (Render, Railway, Fly, ...) tell the program its port through $PORT.
    if (argc == 1) {
        const char* envPort = std::getenv("PORT");
        if (envPort && *envPort && !readPort(envPort, port)) {
            std::fprintf(stderr, "Invalid PORT environment variable: %s\n", envPort);
            return 1;
        }
    }
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
    // Deployment settings (all optional for local use):
    //   BIND_HOST   address to listen on, default 127.0.0.1 (the Docker image sets 0.0.0.0)
    //   APP_USER, APP_PASS   turn on HTTP Basic login for every page and API call
    const std::string bindHost = envOr("BIND_HOST", "127.0.0.1");
    const std::string appUser  = envOr("APP_USER", "");
    const std::string appPass  = envOr("APP_PASS", "");
    const bool authOn = !appUser.empty() && !appPass.empty();
    const bool publicBind = bindHost != "127.0.0.1" && bindHost != "localhost";
    if (publicBind && !authOn) {
        std::fprintf(stderr, "Refusing to listen on %s without a login. Set APP_USER and APP_PASS.\n", bindHost.c_str());
        return 1;
    }
    const std::string expectedAuth = "Basic " + base64Encode(appUser + ":" + appPass);

    svr.set_payload_max_length(64 * 1024);   // no request body here needs more than a few hundred bytes

    if (authOn) {
        svr.set_pre_routing_handler([expectedAuth](const httplib::Request& req, httplib::Response& res) {
            if (sameText(req.get_header_value("Authorization"), expectedAuth)) {
                return httplib::Server::HandlerResponse::Unhandled;
            }
            res.status = 401;
            res.set_header("WWW-Authenticate", "Basic realm=\"ReliefNet\", charset=\"UTF-8\"");
            res.set_content("Login required", "text/plain; charset=utf-8");
            return httplib::Server::HandlerResponse::Handled;
        });
    }

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
        // Behind a hosting proxy the page's origin is the public address, not localhost. A request
        // whose Origin matches the Host it was sent to is the page talking to its own server.
        const std::string host = req.get_header_value("Host");
        if (!host.empty() && (in.origin == "http://" + host || in.origin == "https://" + host)) {
            in.origin.clear();
        }

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

    // Local only unless BIND_HOST says otherwise (and then a login is required, see above).
    if (!svr.bind_to_port(bindHost.c_str(), port)) {
        std::fprintf(stderr, "Cannot listen on port %d (is another program using it?). Try: %s %d\n",
                     port, argv[0], port == kDefaultPort ? 8081 : kDefaultPort);
        return 1;
    }

    std::printf("ReliefNet server: http://%s:%d/   (login %s, Ctrl+C to stop)\n", bindHost.c_str(), port, authOn ? "ON" : "off");
    if (!haveWeb) {
        std::printf("web/ folder not found: only the API and a placeholder page are served.\n");
    }
    std::fflush(stdout);

    svr.listen_after_bind();
    return 0;
}
