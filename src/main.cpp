/*
 * ============================================================================
 *  main.cpp — Entry point of the MTD (MultiThreaded) HTTP Server
 * ============================================================================
 *
 *  This is where everything starts. Here we:
 *      1. Set up signal handling (so Ctrl+C shuts down gracefully)
 *      2. Create the server on port 8080 with 4 worker threads
 *      3. Register all the URL routes (what happens when you visit each page)
 *      4. Start the server and let it run forever
 *
 *  To test:
 *      - Open a browser and go to http://localhost:8080/
 *      - Or use curl:  curl http://localhost:8080/health
 *      - Press Ctrl+C in the terminal to stop the server
 *
 * ============================================================================
 */

#include "Server.h"
#include "ThreadPool.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

// Global pointer to the server — needed so the signal handler can access it
// (Signal handlers are plain C functions, they can't capture variables like lambdas)
Server* gServer = nullptr;

/*
 * onSignal() — Called when the user presses Ctrl+C (SIGINT).
 *
 *  Instead of crashing, we gracefully stop the server so it can:
 *      - Finish handling any in-progress requests
 *      - Shut down the thread pool cleanly
 *      - Close all sockets properly
 */
void onSignal(int) {
    std::cout << "\n[Server] Shutting down...\n";
    if (gServer) gServer->stop();
}

int main() {

    // Register our signal handler for Ctrl+C (SIGINT)
    signal(SIGINT, onSignal);

    try {
        /*
         * Create the server:
         *   - Port 8080: the server listens here for HTTP requests
         *   - 4 threads: the thread pool will have 4 workers handling requests
         */
        Server server(8080, 4);
        gServer = &server;

        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        //  CACHED ROUTES — response is stored in LRU cache after first hit
        //  Same URL hit twice → second request skips the handler entirely
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

        // Home page — simple HTML response
        server.addCachedRoute("GET", "/", [](const HttpRequest&) {
            std::cout << "[Handler] Building / response\n";
            return HttpResponse::makeOk(
                "<h1>Hello from C++ Web Server!</h1>"
                "<p>Multithreaded + LRU Cached</p>"
            );
        });

        // Health check — returns JSON (useful for monitoring tools)
        server.addCachedRoute("GET", "/health", [](const HttpRequest&) {
            std::cout << "[Handler] Building /health response\n";
            return HttpResponse::makeOk(
                R"({"status":"ok","cache":"enabled"})",
                "application/json"
            );
        });

        // Slow route — simulates a 2-second expensive operation
        // First request: takes 2 seconds. Every request after: instant from cache!
        // This is the best demo of how caching saves time
        server.addCachedRoute("GET", "/slow", [](const HttpRequest&) {
            std::cout << "[Handler] /slow — sleeping 2s...\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return HttpResponse::makeOk(
                R"({"result":"slow query done","note":"next hit will be instant"})",
                "application/json"
            );
        });

        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
        //  NON-CACHED ROUTES — handler runs fresh every single time
        // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

        // Echo route — shows back the request details (useful for debugging)
        server.addRoute("GET", "/echo", [](const HttpRequest& req) {
            return HttpResponse::makeOk(
                "<pre>Path: " + req.path +
                "\nMethod: " + req.method + "</pre>"
            );
        });

        // Cache stats — shows live cache metrics as JSON
        // Uses a reference capture [&server] so it can access the server's cache
        server.addRoute("GET", "/cache-stats", [&server](const HttpRequest&) {
            LRUCache& c = server.getCache();

            size_t hits      = c.hits();       // How many cache hits so far
            size_t misses    = c.misses();     // How many cache misses so far
            size_t total     = hits + misses;  // Total lookups
            double hitRate   = total > 0
                               ? (hits * 100.0 / total)   // Hit percentage
                               : 0.0;

            // Build JSON manually — no external library needed!
            std::string json =
                "{\n"
                "  \"hits\": "     + std::to_string(hits)          + ",\n"
                "  \"misses\": "   + std::to_string(misses)        + ",\n"
                "  \"total\": "    + std::to_string(total)         + ",\n"
                "  \"hit_rate\": " + std::to_string(hitRate)       + ",\n"
                "  \"size\": "     + std::to_string(c.size())      + ",\n"
                "  \"capacity\": " + std::to_string(c.capacity())  + "\n"
                "}";

            return HttpResponse::makeOk(json, "application/json");
        });

        // Print out all available routes so the user knows what to visit
        std::cout << "Routes:\n"
                  << "  http://localhost:8080/             (cached)\n"
                  << "  http://localhost:8080/health       (cached)\n"
                  << "  http://localhost:8080/slow         (cached, 2s first hit)\n"
                  << "  http://localhost:8080/echo         (not cached)\n"
                  << "  http://localhost:8080/cache-stats  (live metrics)\n";

        // Start the server! This blocks here forever until stop() is called.
        server.run();

    } catch (const std::exception& e) {
        // If anything goes wrong during setup (port in use, etc.), print the error
        std::cerr << "[Fatal] " << e.what() << "\n";
        return 1;
    }
}