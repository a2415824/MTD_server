/*
 * ============================================================================
 *  Server.cpp — The heart of the HTTP server
 * ============================================================================
 *
 *  This file contains the networking code that makes the server actually work.
 *  Here's the journey of every HTTP request:
 *
 *      1. Client connects       → accept() returns a new socket
 *      2. Thread pool picks it up → handleClient() runs in a worker thread
 *      3. Read the raw request   → recv() gets bytes from the client
 *      4. Parse the request      → HttpRequest::parse() turns bytes into a struct
 *      5. Route the request      → route() finds the matching handler
 *      6. Check cache first      → If cached, skip the handler entirely!
 *      7. Run the handler        → The lambda function returns an HttpResponse
 *      8. Cache the result       → Store it for next time (if it's a cached route)
 *      9. Send the response      → send() pushes bytes back to the client
 *     10. Close the connection   → close() frees the socket file descriptor
 *
 *  NETWORKING CONCEPTS USED:
 *
 *      socket()   → Creates a communication endpoint (like getting a phone)
 *      bind()     → Assigns an address+port to the socket (like getting a phone number)
 *      listen()   → Marks the socket as "ready to receive calls"
 *      accept()   → Waits for and accepts an incoming connection (picks up the phone)
 *      recv()     → Reads data from the client
 *      send()     → Writes data to the client
 *      close()    → Hangs up and frees the resource
 *
 * ============================================================================
 */

#include "Server.h"
#include <iostream>
#include <sys/socket.h>   // socket(), bind(), listen(), accept()
#include <netinet/in.h>   // sockaddr_in, INADDR_ANY, htons()
#include <unistd.h>       // close(), read()
#include <cstring>        // memset()
#include <stdexcept>      // std::runtime_error


// ─── Constructor ────────────────────────────────────────────────────────────
// Sets up the TCP socket, binds it to a port, and starts listening.
// After this, the server is READY but not yet accepting connections.
// You need to call run() to start the accept loop.

Server::Server(int port, int numThreads)
    : port(port), pool(numThreads)
{
    /*
     * socket() — Creates a new network socket.
     *
     *   AF_INET     = We want to use IPv4 (the most common internet protocol)
     *   SOCK_STREAM = We want TCP (reliable, ordered delivery of data)
     *                 (SOCK_DGRAM would be UDP — faster but no delivery guarantees)
     *   0           = Let the OS automatically pick the right protocol (TCP for STREAM)
     *
     *   Returns a file descriptor (just an integer) that we'll use for all
     *   future operations on this socket. Returns -1 on failure.
     */
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        throw std::runtime_error("socket() failed");
    }

    /*
     * SO_REUSEADDR — Allows us to restart the server immediately.
     *
     *   Without this option, if you stop and restart the server quickly,
     *   bind() will fail with "Address already in use" for about 60 seconds.
     *   This happens because the OS keeps the port in a "TIME_WAIT" state
     *   after the previous server closes. SO_REUSEADDR tells the OS:
     *   "It's fine, let me reuse this port even if it's still in TIME_WAIT."
     */
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /*
     * sockaddr_in — Describes WHERE we want to listen.
     *
     *   sin_family      = AF_INET means IPv4
     *   sin_addr.s_addr = INADDR_ANY means "listen on ALL network interfaces"
     *                     (localhost, WiFi, Ethernet — all of them)
     *   sin_port        = htons(port) converts the port number to "network byte order"
     *
     *   WHY htons()? Network protocols use big-endian byte order, but most
     *   CPUs (x86, ARM) use little-endian. htons() flips the bytes so the
     *   port number is correctly understood by other machines on the network.
     */
    sockaddr_in addr{};
    memset(&addr, 0, sizeof(addr));        // Zero out the struct to avoid garbage data
    addr.sin_family      = AF_INET;        // IPv4
    addr.sin_addr.s_addr = INADDR_ANY;     // Listen on all interfaces (0.0.0.0)
    addr.sin_port        = htons(port);    // Convert port to network byte order

    /*
     * bind() — Attaches our socket to the address and port.
     *   After this, the OS knows that data arriving on this port should
     *   be delivered to our socket.
     */
    if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("bind() failed — is port already in use?");
    }

    /*
     * listen() — Tells the OS we're ready to accept incoming connections.
     *
     *   The second argument (10) is the "backlog" — the maximum number of
     *   connections that can wait in line while we're busy handling others.
     *   If the backlog is full, the OS rejects new connections with a TCP RST.
     */
    if (listen(serverFd, 10) < 0) {
        throw std::runtime_error("listen() failed");
    }

    std::cout << "[Server] Listening on port " << port << "\n";
}


// ─── Destructor ─────────────────────────────────────────────────────────────
// Stops the server and closes the listening socket.

Server::~Server() {
    stop();
    if (serverFd >= 0) close(serverFd);    // Close our listening socket if it's open
}

// Simply sets the running flag to false — the accept loop will exit on next iteration
void Server::stop() { running = false; }


// ─── Route Registration ──────────────────────────────────────────────────────
// These methods let you tell the server "when someone visits /hello, do THIS"

/*
 * addRoute() — Registers a handler for a specific method+path.
 *   The key is formed by concatenating method and path: "GET /hello"
 *   This makes lookup O(1) using the unordered_map.
 */
void Server::addRoute(const std::string& method,
                      const std::string& path,
                      Handler handler)
{
    routes[method + " " + path] = std::move(handler);
}

/*
 * addCachedRoute() — Like addRoute(), but also marks this route for caching.
 *   The route key is added to the cachedRoutes set so the router knows
 *   to check the cache before calling the handler.
 */
void Server::addCachedRoute(const std::string &method, const std::string &path, Handler handler) {
    std::string key = method+" "+path;
    routes[key] = std::move(handler);
    cachedRoutes.insert(key);          // Mark this route as "should be cached"
}


// ─── Main Accept Loop ────────────────────────────────────────────────────────
// This is the infinite loop that waits for clients to connect.

void Server::run() {
    running = true;
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        /*
         * accept() — The server pauses here and waits for a client to connect.
         *
         *   When a client connects, accept() returns a NEW file descriptor
         *   just for that client. The original serverFd stays open and keeps
         *   listening for more clients. Think of it like a receptionist:
         *   they greet each visitor and assign them to a specific room (clientFd).
         */
        int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);

        if (clientFd < 0) {
            if (!running) break;   // stop() was called — this is expected, not an error
            std::cerr << "[Server] accept() error\n";
            continue;
        }

        /*
         * Hand off to the thread pool!
         *
         *   Instead of handling this client right here (which would block
         *   the accept loop), we push the work to a worker thread.
         *   This way, accept() can immediately loop back and wait for
         *   the next client. This is the ENTIRE POINT of the thread pool!
         */
        pool.enqueue([this, clientFd] {
            handleClient(clientFd);
            close(clientFd);   // ALWAYS close — forgetting this = resource leak!
        });
    }
}


// ─── Per-Client Handler (runs in a worker thread) ────────────────────────────
// This function handles ONE client from start to finish.

void Server::handleClient(int clientFd) {

    /*
     * Read the raw HTTP request from the client.
     *
     *   We create a 4KB buffer on the stack — plenty for typical HTTP headers.
     *   The "= {}" zero-initializes the buffer to avoid reading garbage data
     *   if the client sends fewer bytes than the buffer can hold.
     */
    char buffer[4096] = {};

    /*
     * recv() — Reads data from the client socket into our buffer.
     *
     *   sizeof(buffer) - 1 : Read at most 4095 bytes, leaving room for a
     *                        null terminator so we can treat it as a C string.
     *   Returns the number of bytes read, 0 if disconnected, -1 on error.
     */
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) return;  // Client disconnected or error — nothing to do

    // Parse the raw text into a clean HttpRequest struct
    HttpRequest req;
    if (!HttpRequest::parse(std::string(buffer, bytesRead), req)) {
        // Parsing failed — the request was malformed. Send a 400 Bad Request.
        auto resp = HttpResponse::makeError(400, "Bad Request");
        auto raw  = resp.toString();
        send(clientFd, raw.c_str(), raw.size(), 0);
        return;
    }

    // Route the request → get the response (may come from cache or handler)
    HttpResponse resp = route(req);
    std::string raw   = resp.toString();

    /*
     * Send the response back to the client.
     *
     *   send() might not send ALL bytes in one call (this is called a
     *   "partial send" and happens on slow or congested networks).
     *   So we loop until every byte has been delivered.
     */
    size_t totalSent = 0;
    while (totalSent < raw.size()) {
        ssize_t sent = send(clientFd,
                            raw.c_str() + totalSent,     // Start from where we left off
                            raw.size() - totalSent,      // Only send remaining bytes
                            0);
        if (sent <= 0) break;  // Connection dropped mid-send — give up
        totalSent += sent;
    }
}


// ─── Router ─────────────────────────────────────────────────────────────────
// Matches a request to the right handler and handles caching.

HttpResponse Server::route(const HttpRequest& req) {

    // Build the lookup key: "GET /hello"
    std::string key = req.method + " " + req.path;

    // Check if this route should be cached
    bool shouldCache = (req.method == "GET")&&(cachedRoutes.count(key)>0);

    if (shouldCache) {
        // ── Try the cache first ──
        auto cached = cache.get(key);

        if (cached.has_value()) {
            // Cache HIT! We found a stored response — no need to run the handler
            std::cout<<"[Cache HIT]"<<key<<"\n";

            HttpResponse resp;
            resp.statusCode = 200;
            resp.statusText = "OK";
            resp.body = cached.value();
            resp.headers["X-Cache"] = "HIT";    // Custom header so you can verify caching works
            return resp;
        }
        // Cache MISS — we need to generate a fresh response
        std::cout<<"[Cache Miss]"<<key<<"\n";
    }

    // Look up the handler function for this route
    auto it = routes.find(key);
    if (it == routes.end()) {
        // No handler registered for this URL — send 404 Not Found
        return HttpResponse::makeError(404, "Not Found");
    }

    // Run the handler to generate a response
    HttpResponse resp = it->second(req);

    // If this is a cached route and the response was successful, store it
    if (shouldCache && resp.statusCode == 200) {
        cache.put(key,resp.body);
    }

    return resp;
}