#include "SocketUtils.h"
#include "Types.h"
#include "Levenshtein.h"

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
#endif

#include <limits>
#include <string>
#include <string_view>

// ─────────────────────────────────────────────────────────────────────────────
// Known routes — mirrors routes.cgua / routes_generated.hpp.
// TODO: replace with RouteTable from routes_generated.hpp once Request/Response
//       types are fully defined and the two halves are wired together.
// ─────────────────────────────────────────────────────────────────────────────
struct RouteEntry { std::string_view method; std::string_view path; };

static constexpr RouteEntry known_routes[] = {
    {"GET",    "api/users"},
    {"POST",   "api/users"},
    {"GET",    "api/users/:id"},
    {"PUT",    "api/users/:id"},
    {"DELETE", "api/users/:id"},
    {"GET",    "health"},
    {"POST",   "api/auth/login"},
    {"POST",   "api/auth/logout"},
};

// ─────────────────────────────────────────────────────────────────────────────
// parseRequestLine
//
// Extracts method and path from the first line of a raw HTTP request.
// Input example: "GET /api/users HTTP/1.1\r\n..."
// Returns {"GET", "api/users"} (leading '/' stripped to match route table).
// ─────────────────────────────────────────────────────────────────────────────
static std::pair<std::string, std::string> parseRequestLine(const std::string& raw) {
    std::size_t s1 = raw.find(' ');
    if (s1 == std::string::npos) return {};
    std::size_t s2 = raw.find(' ', s1 + 1);
    if (s2 == std::string::npos) return {};

    std::string method = raw.substr(0, s1);
    std::string path   = raw.substr(s1 + 1, s2 - s1 - 1);

    if (!path.empty() && path[0] == '/') path = path.substr(1);  // strip leading '/'

    return {method, path};
}

// ─────────────────────────────────────────────────────────────────────────────
// suggestRoute
//
// Computes Levenshtein distance between the incoming "METHOD path" string and
// every known route, then returns the closest match as a human-readable hint.
// Combining method + path into one string means a wrong method is penalised
// naturally alongside a wrong path.
// ─────────────────────────────────────────────────────────────────────────────
static std::string suggestRoute(const std::string& method, const std::string& path) {
    std::string query = method + " /" + path;

    std::size_t     minDist    = std::numeric_limits<std::size_t>::max();
    const RouteEntry* best     = nullptr;

    for (const auto& r : known_routes) {
        std::string candidate = std::string(r.method) + " /" + std::string(r.path);
        std::size_t d = Levenshtein::distance(query, candidate);
        if (d < minDist) {
            minDist = d;
            best    = &r;
        }
    }

    if (!best) return "";
    return std::string(best->method) + " /" + std::string(best->path);
}

void Cgua::App::listen(std::string port) {
    Cgua::Socket servsock = SocketUtils::create_server_socket(port);
    _listen_sockfd = servsock.sockfd;

    if (bind(servsock.sockfd, servsock.servinfo->ai_addr, servsock.servinfo->ai_addrlen) < 0)
        SocketUtils::error_n_die("Bind error");

    if (::listen(servsock.sockfd, 128) < 0)
        SocketUtils::error_n_die("Socket Error");

    freeaddrinfo(servsock.servinfo);

    std::printf("C-gua is listening on port %s...\n", port.c_str());
    this->start_loop();
}

void Cgua::App::start_loop() {
    while (true) {
        sockaddr_storage their_addr;
        socklen_t        addr_size = sizeof(their_addr);

        int connected_sockfd = accept(_listen_sockfd, (sockaddr*)&their_addr, &addr_size);

        // ── Receive ───────────────────────────────────────────────────────────
        char buf[4096];
        int  recv_status = recv(connected_sockfd, buf, sizeof(buf) - 1, 0);

        if (recv_status < 0) {
            SocketUtils::error_n_die("Error receiving from the client", recv_status);
        } else if (recv_status == 0) {
            SocketUtils::error_n_die("Client closed connection!", recv_status);
        }

        buf[recv_status] = '\0';
        std::string raw_request(buf, recv_status);

        // ── Route matching ────────────────────────────────────────────────────
        auto [method, path] = parseRequestLine(raw_request);

        bool matched = false;
        for (const auto& r : known_routes) {
            if (r.method == method && r.path == path) {
                matched = true;
                break;
            }
        }

        // ── Build response ────────────────────────────────────────────────────
        HttpResponse res;

        if (matched) {
            res.status = "200 OK";
            res.body   = "GNU was here!";
        } else {
            std::string suggestion = suggestRoute(method, path);
            res.status      = "404 Not Found";
            res.contentType = "text/plain";
            res.body        = "404 Not Found: " + method + " /" + path + "\r\n" +
                              "Did you mean: " + suggestion + "?";
        }

        // ── Send ──────────────────────────────────────────────────────────────
        std::string raw_res = res.toString();
        if (send(connected_sockfd, raw_res.c_str(), raw_res.length(), 0) < 0)
            SocketUtils::error_n_die("Error sending message");

        close(connected_sockfd);
    }
}
