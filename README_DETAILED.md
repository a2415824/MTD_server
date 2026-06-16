# MTD Server — The Full Story 📖

## How a C++ Web Server Works, Explained Like a Story

This document walks you through every component of the MTD Server project. Instead of dry technical jargon, we'll explain things the way you'd tell a friend — with analogies, step-by-step breakdowns, and a clear story from start to finish.

---

## Table of Contents

1. [The Big Picture — What Are We Building?](#the-big-picture)
2. [Chapter 1: The Server — Our Restaurant](#chapter-1-the-server)
3. [Chapter 2: The Thread Pool — Our Team of Waiters](#chapter-2-the-thread-pool)
4. [Chapter 3: The HTTP Request — Reading the Customer's Order](#chapter-3-the-http-request)
5. [Chapter 4: The HTTP Response — Serving the Meal](#chapter-4-the-http-response)
6. [Chapter 5: The LRU Cache — Our Speed Dial Menu](#chapter-5-the-lru-cache)
7. [Chapter 6: main.cpp — Opening Night](#chapter-6-main-cpp)
8. [The Complete Journey — A Request from Start to Finish](#the-complete-journey)
9. [Key C++ Concepts Used](#key-cpp-concepts)

---

## The Big Picture

Imagine you're building a restaurant from scratch. Not just any restaurant — one that can serve many customers at the same time, remembers popular orders so it can serve them faster, and runs smoothly even when things get busy.

That's exactly what this project does, except instead of food, we serve **web pages**. Instead of customers walking in, we have **browsers sending HTTP requests**. And instead of a kitchen, we have **C++ functions that generate responses**.

Here's the cast of characters:

| Component | Restaurant Analogy | What It Actually Does |
|-----------|-------------------|----------------------|
| **Server** | The restaurant building itself | Listens for connections, accepts clients, coordinates everything |
| **ThreadPool** | The team of waiters | Multiple workers handling clients simultaneously |
| **HttpRequest** | The customer's order slip | Parses what the client is asking for |
| **HttpResponse** | The plated meal | Builds the reply to send back |
| **LRUCache** | The speed dial menu | Remembers recent responses to serve them instantly |

---

## Chapter 1: The Server

### `Server.h` and `Server.cpp`

**The restaurant analogy:** The Server is your entire restaurant building — the front door where customers enter, the reception desk that greets them, and the system that assigns each customer to a waiter.

### What happens when the server starts up?

Think of it like opening a restaurant for the evening:

```
Step 1: Get a phone (socket)
     → socket() creates a communication endpoint
     → Like getting a phone to receive reservation calls

Step 2: Get a phone number (bind)
     → bind() assigns our address and port number
     → Like listing our restaurant's phone number in the directory

Step 3: Start answering calls (listen)
     → listen() tells the OS we're ready for connections
     → Like telling the phone company "we're open, send calls our way"

Step 4: Wait for customers (accept loop)
     → accept() blocks and waits for a client to connect
     → Like a host standing at the door, waiting to greet the next guest
```

### The Accept Loop — The Heart of the Server

```cpp
while (running) {
    int clientFd = accept(serverFd, ...);  // Wait for a customer
    pool.enqueue([this, clientFd] {        // Assign them to a waiter
        handleClient(clientFd);            // The waiter handles everything
        close(clientFd);                   // Customer leaves, table is freed
    });
}
```

This is the core loop. It does three things on repeat:
1. **Wait** for a client to connect (`accept`)
2. **Hand off** the client to a worker thread (`pool.enqueue`)
3. **Loop back** immediately to wait for the next client

The key insight: the main thread **never** handles a client directly. It just accepts connections and delegates. This is why we can handle hundreds of clients — the main thread is always free to accept the next one.

### Route Registration — Setting Up the Menu

Before the server starts accepting clients, we tell it what to do for each URL:

```cpp
// "When someone visits /hello, run this function"
server.addRoute("GET", "/hello", [](const HttpRequest&) {
    return HttpResponse::makeOk("<h1>Hello!</h1>");
});
```

Internally, routes are stored in a hash map with keys like `"GET /hello"`. When a request comes in, the server builds the same key from the request and looks it up — O(1) lookup, instant!

### Cached vs. Non-Cached Routes

The server supports two types of routes:

- **`addRoute()`** — The handler runs **every single time**. Good for dynamic content that changes per request (like echoing request info).

- **`addCachedRoute()`** — The handler runs **once**, and the result is stored in the LRU cache. Future requests for the same URL get the cached response instantly. Good for expensive operations that don't change often.

### Files

- **[Server.h](src/Server/Server.h)** — Declares the Server class with all its members
- **[Server.cpp](src/Server/Server.cpp)** — The actual implementation (socket setup, accept loop, routing)

---

## Chapter 2: The Thread Pool

### `ThreadPool.h` and `ThreadPool.cpp`

**The restaurant analogy:** If the Server is the restaurant, the ThreadPool is your team of waiters. You hire them all on opening night, and they wait in the break room until a customer arrives. When a new customer comes in, one waiter wakes up and takes care of them.

### Why not just create a new thread for every client?

Great question! Here's the problem:

```
Creating a thread  = Hiring a new waiter for EVERY customer
                   = Expensive! (takes time, uses memory, OS has limits)
                   = Imagine hiring and firing a waiter every 2 minutes

Thread pool        = Hire 4 waiters once, they handle ALL customers
                   = Efficient! (no hiring/firing overhead)
                   = The waiters just grab the next customer when they're free
```

### How does it work inside?

The thread pool has three key parts:

**1. A task queue** (the order queue)
```
Tasks waiting to be picked up:
    [Handle Client #1] → [Handle Client #2] → [Handle Client #3]
```

**2. Worker threads** (the waiters)
Each worker runs this loop forever:
```
    → Sleep until there's a task in the queue
    → Wake up
    → Grab the task from the front of the queue
    → Do the work
    → Go back to sleep
```

**3. A condition variable** (the bell)
Instead of workers constantly checking "is there work? is there work?", they sleep peacefully. When a new task arrives, `notify_one()` rings a bell that wakes up exactly one sleeping worker. No wasted CPU!

### The Synchronization Dance

Multiple threads accessing the same queue is dangerous — it can cause crashes, data corruption, and bugs that are nearly impossible to debug. Here's how we keep things safe:

```
mutex            = A bathroom lock. Only one person at a time.
                   Only one thread can access the task queue at once.

condition_variable = A doorbell. Instead of knocking every second,
                     you ring it once and the person inside wakes up.

unique_lock      = A smart key that automatically unlocks the door
                   when you leave the room (even if you forget).
```

### Files

- **[ThreadPool.h](src/ThreadPool/ThreadPool.h)** — Declares the pool with worker vector, task queue, and sync primitives
- **[ThreadPool.cpp](src/ThreadPool/ThreadPool.cpp)** — Creates workers, implements enqueue and clean shutdown

---

## Chapter 3: The HTTP Request

### `HttpRequest.h` and `HttpRequest.cpp`

**The restaurant analogy:** When a customer walks in and says "I'd like a chicken sandwich with no mayo, and a side of fries", the waiter doesn't memorize the whole sentence. They write it down on an order slip with separate fields: MAIN: chicken sandwich, MODIFICATIONS: no mayo, SIDE: fries. That's what `HttpRequest::parse()` does — it takes the customer's raw words and fills out a clean order slip.

### What does raw HTTP look like?

When a browser visits `http://localhost:8080/hello`, it sends text like this over the network:

```
GET /hello HTTP/1.1\r\n
Host: localhost:8080\r\n
Accept: text/html\r\n
\r\n
```

That's it! HTTP is just text. Each line ends with `\r\n` (carriage return + newline — a holdover from the typewriter era).

### How we parse it

The `parse()` method reads this text line by line:

```
Line 1: "GET /hello HTTP/1.1"
         ↓     ↓       ↓
       method  path  version

Lines 2+: "Host: localhost:8080"
            ↓         ↓
           key      value     →  stored in headers map

Blank line: signals "headers are done"

Everything after: the body (empty for GET, filled for POST)
```

### The result

After parsing, we have a clean struct:

```cpp
HttpRequest req;
req.method  = "GET";
req.path    = "/hello";
req.version = "HTTP/1.1";
req.headers = { {"Host", "localhost:8080"}, {"Accept", "text/html"} };
req.body    = "";
```

Now the rest of our code can easily check `req.path` to decide what to do, instead of searching through raw text. Much cleaner!

### Files

- **[HttpRequest.h](src/HttpRequest/HttpRequest.h)** — Defines the struct with method, path, version, headers, and body
- **[HttpRequest.cpp](src/HttpRequest/HttpRequest.cpp)** — The parsing logic that reads raw HTTP line by line

---

## Chapter 4: The HTTP Response

### `HttpResponse.h` and `HttpResponse.cpp`

**The restaurant analogy:** After the kitchen prepares the meal, it needs to be plated nicely, garnished, and delivered to the customer. The `HttpResponse` class does exactly that — it takes our content (HTML, JSON) and wraps it in a properly formatted HTTP response that browsers can understand.

### What does a response look like?

When we send a response back, it needs to be in this exact format:

```
HTTP/1.1 200 OK\r\n
Content-Type: text/html\r\n
Content-Length: 45\r\n
Connection: close\r\n
\r\n
<h1>Hello from C++ Web Server!</h1>
```

### The three parts of every response

```
Part 1: Status Line     → "HTTP/1.1 200 OK"
                           Tells the browser if the request succeeded or failed
                           200 = success, 404 = not found, 500 = server error

Part 2: Headers          → Key-value metadata about the response
   Content-Type          → What kind of data? (text/html, application/json)
   Content-Length        → How many bytes is the body?
   Connection: close     → "I'll close the connection after sending this"

Part 3: Body             → The actual content the user sees
                           HTML for web pages, JSON for APIs
```

### Factory methods — shortcuts for common responses

Instead of setting every field manually, we have helper methods:

```cpp
// Quick success response — sets all headers automatically
HttpResponse::makeOk("<h1>Hello!</h1>");

// Quick error response — creates a simple HTML error page
HttpResponse::makeError(404, "Not Found");
// Produces: <h1>404 Not Found</h1>
```

### Files

- **[HttpResponse.h](src/HttpResponse/HttpResponse.h)** — Declares the class with status, headers, body, and factory methods
- **[HttpResponse.cpp](src/HttpResponse/HttpResponse.cpp)** — toString() serialization and makeOk/makeError factories

---

## Chapter 5: The LRU Cache

### `LRUCache.h` and `LRUCahe.cpp`

**The restaurant analogy:** Your most popular dishes take a long time to prepare. Instead of cooking them from scratch every time, you keep a few pre-made portions in a warming tray. When someone orders a popular dish, you grab it from the tray instantly. But the tray only fits 5 dishes — when a new one goes in, the one that's been sitting there the longest (least ordered recently) gets tossed out. That's an LRU Cache!

### The problem it solves

Imagine the `/slow` route takes 2 seconds to compute. If 100 people visit it:

```
Without cache:   100 × 2 seconds = 200 seconds of computation 😱
With LRU cache:    1 × 2 seconds = 2 seconds  (99 served from cache!) 🚀
```

### The clever data structure

The LRU Cache uses **two data structures working together**:

```
┌──────────────────────────────────────────────────────────┐
│  Hash Map (unordered_map)                                │
│  ┌─────────────┬──────────────────────────┐              │
│  │ Key         │ Points to ↓              │              │
│  ├─────────────┼──────────────────────────┤              │
│  │ "GET /"     │ ──→ [Node in list]       │              │
│  │ "GET /slow" │ ──→ [Node in list]       │              │
│  │ "GET /health│ ──→ [Node in list]       │              │
│  └─────────────┴──────────────────────────┘              │
│                                                          │
│  Doubly-Linked List (ordered by recent use)              │
│  ┌────────────┐   ┌────────────┐   ┌────────────┐       │
│  │ "GET /"    │←→│ "GET /slow" │←→│"GET /health"│       │
│  │ (newest)   │   │             │   │ (oldest)   │       │
│  └────────────┘   └────────────┘   └────────────┘       │
│   ↑ FRONT                              BACK ↑           │
│   Most recently used          Least recently used        │
└──────────────────────────────────────────────────────────┘
```

**Why two structures?**

- **Hash map alone**: O(1) lookup, but no way to know which item is "least recently used"
- **Linked list alone**: Easy to track usage order, but O(n) to find an item
- **Both together**: O(1) lookup AND O(1) usage tracking — best of both worlds!

### The key operations

**`get(key)`** — Looking up a cached response:
```
1. Look up the key in the hash map           → O(1)
2. Not found? → Cache MISS, return empty
3. Found but expired (past TTL)? → Remove it, return empty
4. Found and fresh? → Move it to the front (it's "recently used" now)
                    → Cache HIT! Return the value
```

**`put(key, value)`** — Storing a response:
```
1. Key already exists? → Update the value, move to front
2. Key is new:
   a. Cache full? → Remove the item at the BACK (least recently used)
   b. Add the new item to the FRONT
   c. Add it to the hash map
```

### TTL (Time-To-Live) — Nothing lives forever

Each cached entry has a timestamp of when it was stored. When we look it up, we check:

```
Time now - Time when cached  ≥  TTL seconds?
    YES → This entry is STALE. Delete it, treat as a cache miss.
    NO  → This entry is FRESH. Serve it!
```

In our server, TTL is 30 seconds. So a cached response is served instantly for 30 seconds, then the next request regenerates it fresh.

### Thread safety

Multiple worker threads might try to read/write the cache at the same time. That's a recipe for disaster without protection:

```
Thread 1: Reading the list...
Thread 2: Modifying the list at the same time!
Result:   💥 Crash, corruption, or wrong data
```

Solution: A **mutex** (mutual exclusion lock). Before any thread touches the cache, it must acquire the lock. If another thread already has it, the second thread waits its turn.

### Files

- **[LRUCache.h](src/LRUCache/LRUCache.h)** — Class declaration with list, map, mutex, and atomic counters
- **[LRUCahe.cpp](src/LRUCache/LRUCahe.cpp)** — Implementation of get(), put(), clear(), and isExpired()

---

## Chapter 6: main.cpp

### `main.cpp`

**The restaurant analogy:** This is opening night! The owner (you) walks in, sets up the menu, hires the staff, and flips the "OPEN" sign. Everything we built in the previous chapters comes together here.

### What happens step by step

```
1. Set up signal handling
   → Register a handler for Ctrl+C (SIGINT)
   → When pressed, it calls server.stop() instead of crashing
   → This ensures graceful shutdown (finish current requests, close sockets)

2. Create the server
   → Server server(8080, 4);
   → Opens a socket on port 8080
   → Creates a thread pool with 4 worker threads
   → The server is now listening but not yet accepting connections

3. Register routes (set up the menu)
   → addCachedRoute("GET", "/", ...)         ← Home page (cached)
   → addCachedRoute("GET", "/health", ...)   ← Health check (cached)
   → addCachedRoute("GET", "/slow", ...)     ← Slow route (cached — this is the cache demo!)
   → addRoute("GET", "/echo", ...)           ← Echo route (NOT cached)
   → addRoute("GET", "/cache-stats", ...)    ← Cache metrics (NOT cached — it needs to be live)

4. Start the server
   → server.run();
   → This enters the infinite accept loop
   → The program stays here until Ctrl+C is pressed
```

### Why some routes are cached and others aren't

| Route | Cached? | Why? |
|-------|---------|------|
| `/` | ✅ Yes | Static content — doesn't change between requests |
| `/health` | ✅ Yes | Health status rarely changes — cache is fine |
| `/slow` | ✅ Yes | Takes 2 seconds! Caching makes it instant after the first call |
| `/echo` | ❌ No | Needs to show the CURRENT request — can't use old cached data |
| `/cache-stats` | ❌ No | Must show LIVE numbers — cached data would be stale instantly |

### File

- **[main.cpp](src/main.cpp)** — The entry point that wires everything together

---

## The Complete Journey

Let's follow a single HTTP request from start to finish. You type `http://localhost:8080/slow` in your browser:

```
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  Browser                                                             │
│  └─→ Creates a TCP connection to localhost:8080                      │
│  └─→ Sends: "GET /slow HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"    │
│                                                                      │
│  Server (main thread)                                                │
│  └─→ accept() returns a new clientFd for this connection             │
│  └─→ pool.enqueue(handleClient) — hands it to a worker thread        │
│  └─→ Immediately loops back to accept() for the next client          │
│                                                                      │
│  ThreadPool (worker thread wakes up)                                 │
│  └─→ Grabs the task from the queue                                   │
│  └─→ Calls handleClient(clientFd)                                   │
│                                                                      │
│  handleClient()                                                      │
│  └─→ recv() reads the raw HTTP text from the client                  │
│  └─→ HttpRequest::parse() turns it into a clean struct               │
│  └─→ route() is called with the parsed request                       │
│                                                                      │
│  route()                                                             │
│  └─→ Builds key: "GET /slow"                                        │
│  └─→ Checks: is this a cached route? YES                            │
│  └─→ cache.get("GET /slow")                                         │
│      ├─→ FIRST TIME: Cache MISS                                     │
│      │   └─→ Runs the handler (sleeps 2 seconds, returns JSON)       │
│      │   └─→ cache.put("GET /slow", response) — stores for later     │
│      │   └─→ Returns the response                                   │
│      └─→ SECOND TIME: Cache HIT                                     │
│          └─→ Returns cached response instantly (< 1ms!)              │
│                                                                      │
│  handleClient() (continued)                                          │
│  └─→ resp.toString() converts to raw HTTP format                     │
│  └─→ send() pushes all bytes to the client                          │
│  └─→ close(clientFd) — connection closed, socket freed               │
│                                                                      │
│  Browser                                                             │
│  └─→ Receives the response and displays it                          │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Key C++ Concepts

Here's a quick reference for the C++ features used throughout this project:

### `std::function` and Lambdas
```cpp
// A lambda is an anonymous function — a function without a name
auto greet = [](const std::string& name) {
    return "Hello, " + name + "!";
};

// std::function can hold any callable: lambdas, regular functions, etc.
std::function<std::string(const std::string&)> fn = greet;
```
Used in: Route handlers, thread pool tasks

### `std::mutex` and `std::unique_lock`
```cpp
std::mutex mtx;

{
    std::unique_lock<std::mutex> lock(mtx);  // Locks the mutex
    // Only one thread can be in here at a time
    // ...critical section...
}  // Lock is automatically released when `lock` goes out of scope
```
Used in: ThreadPool (protecting task queue), LRUCache (protecting cache data)

### `std::condition_variable`
```cpp
std::condition_variable cv;

// Thread 1: Wait until condition is true
cv.wait(lock, []{ return !tasks.empty(); });

// Thread 2: Signal that something changed
cv.notify_one();   // Wake up ONE waiting thread
cv.notify_all();   // Wake up ALL waiting threads
```
Used in: ThreadPool (workers sleep until a task arrives)

### `std::atomic`
```cpp
std::atomic<bool> stop{false};  // Thread-safe boolean — no mutex needed!
stop = true;                     // Safe to read/write from any thread
```
Used in: ThreadPool (stop flag), LRUCache (hit/miss counters)

### `std::optional`
```cpp
std::optional<std::string> result = cache.get("key");
if (result.has_value()) {
    // Found it! Use result.value()
} else {
    // Not found — result is std::nullopt (like null)
}
```
Used in: LRUCache::get() — returns the value if found, or "nothing" if not

### `std::move` (Move Semantics)
```cpp
// Instead of COPYING a big object (expensive), MOVE it (fast)
tasks.push(std::move(task));  // Transfers ownership, no copy made
```
Used everywhere to avoid unnecessary copies of strings, functions, and objects

### RAII (Resource Acquisition Is Initialization)
```
The idea: Tie resource cleanup to object lifetime.
When the object is destroyed, the resource is automatically freed.

Examples in this project:
  - unique_lock → automatically unlocks the mutex when it goes out of scope
  - Server destructor → automatically closes the socket
  - ThreadPool destructor → automatically joins all threads
```

---

## 🎓 What You Can Learn From This Project

By studying this codebase, you'll understand:

1. **Socket programming** — How computers talk to each other over a network
2. **HTTP protocol** — The language that browsers and servers speak
3. **Multithreading** — Running code in parallel without crashing
4. **Synchronization** — Mutex, condition variables, atomics — keeping threads safe
5. **Data structures** — LRU Cache = linked list + hash map (a classic interview question!)
6. **Modern C++** — Lambdas, move semantics, RAII, std::optional, std::function
7. **Software architecture** — Separating concerns into clean, modular components

Happy coding! 🚀
