/*
 * ============================================================================
 *  Server.h — The main HTTP server that ties everything together
 * ============================================================================
 *
 *  This is the brain of the whole project. The Server class:
 *
 *      1. Opens a TCP socket and listens for incoming connections
 *      2. Accepts client connections and hands them to the ThreadPool
 *      3. Reads HTTP requests, matches them to registered routes
 *      4. Uses the LRU Cache to avoid re-computing expensive responses
 *      5. Sends the HTTP response back to the client
 *
 *  ROUTE REGISTRATION:
 *
 *      You tell the server what to do for each URL by registering routes:
 *
 *          server.addRoute("GET", "/hello", [](const HttpRequest&) {
 *              return HttpResponse::makeOk("<h1>Hello!</h1>");
 *          });
 *
 *      The lambda function is your "handler" — it receives the request
 *      and returns a response. Simple as that!
 *
 *  CACHED vs NON-CACHED ROUTES:
 *
 *      addRoute()       → Handler runs EVERY time the URL is requested
 *      addCachedRoute() → Handler runs ONCE, then the response is cached
 *                         and served instantly for future requests
 *
 * ============================================================================
 */

#pragma once
#include "ThreadPool.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <string>
#include <functional>
#include <LRUCache.h>
#include <unordered_set>

class Server {
    public:

     // Type alias: a Handler is any function that takes a request and returns a response
     using Handler = std::function<HttpResponse(const HttpRequest&)>;

    /*
     * Constructor — Sets up the server on the given port.
     *
     *   port       : which port to listen on (e.g., 8080)
     *   numThreads : how many worker threads to create (default: 4)
     *
     *   This also creates the TCP socket, binds it, and starts listening.
     */
    Server(int port ,int numThreads = 4);

    /*
     * Destructor — Closes the socket and shuts everything down cleanly.
     */
    ~Server();

    /*
     * addRoute() — Register a URL handler (NOT cached).
     *   The handler runs fresh every time this URL is requested.
     */
    void addRoute(const std::string& method,const std::string& path,Handler handler);

    /*
     * addCachedRoute() — Register a URL handler (WITH caching).
     *   The handler runs once, and the response is cached for future requests.
     */
    void addCachedRoute(const std::string& method,const std::string& path,Handler handler);

    /*
     * run() — Starts the main accept loop.
     *   Blocks forever (until stop() is called), accepting client connections.
     */
    void run();

    /*
     * stop() — Signals the server to stop accepting new connections.
     */
    void stop();

    /*
     * getCache() — Returns a reference to the LRU cache.
     *   Used by the /cache-stats route to display live cache metrics.
     */
    LRUCache& getCache(){return cache;}

 private:
    int port ;                 // The port number we're listening on
    int serverFd = -1;         // File descriptor for the listening socket (-1 = not yet created)
    bool running = false;      // Controls the accept loop in run()

    ThreadPool pool;           // Our pool of worker threads

    // Routes map: "GET /hello" → handler function
    std::unordered_map<std::string, Handler> routes;

    // LRU Cache: stores responses for cached routes (capacity=100, TTL=30 seconds)
    LRUCache cache{100,30};

    // Set of route keys that should use the cache (e.g., "GET /", "GET /slow")
    std::unordered_set<std::string> cachedRoutes;

    /*
     * handleClient() — Reads the request, routes it, sends the response.
     *   Runs inside a worker thread (not the main thread).
     */
    void handleClient(int clientFd);

    /*
     * route() — Matches a request to its handler and returns the response.
     *   Also handles cache lookup/storage for cached routes.
     */
    HttpResponse route(const HttpRequest& req);

};