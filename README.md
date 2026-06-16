# MTD Server — Multithreaded HTTP Server with LRU Cache

A lightweight, multithreaded HTTP server built from scratch in **C++20** using POSIX sockets. Features a thread pool for concurrent request handling and an LRU cache with TTL for fast response delivery.

## ✨ Features

- **Multithreaded** — Handles multiple clients simultaneously using a thread pool
- **LRU Cache** — Caches responses to avoid recomputing expensive operations
- **TTL Expiry** — Cached entries automatically expire after 30 seconds
- **Route System** — Clean API for registering URL handlers (cached and non-cached)
- **Cache Metrics** — Live stats endpoint showing hits, misses, and hit rate
- **Graceful Shutdown** — Ctrl+C cleanly stops the server

## 📁 Project Structure

```
MTD_server/
├── CMakeLists.txt                  # Build configuration
├── README.md                       # This file (quick start guide)
├── README_DETAILED.md              # In-depth component explanation
└── src/
    ├── main.cpp                    # Entry point — sets up routes and starts the server
    ├── Server/
    │   ├── Server.h                # Server class declaration
    │   └── Server.cpp              # Socket setup, accept loop, routing logic
    ├── ThreadPool/
    │   ├── ThreadPool.h            # Thread pool declaration
    │   └── ThreadPool.cpp          # Worker threads, task queue, synchronization
    ├── HttpRequest/
    │   ├── HttpRequest.h           # HTTP request struct
    │   └── HttpRequest.cpp         # Raw HTTP text parser
    ├── HttpResponse/
    │   ├── HttpResponse.h          # HTTP response class
    │   └── HttpResponse.cpp        # Response builder and factory methods
    └── LRUCache/
        ├── LRUCache.h              # LRU cache declaration
        └── LRUCahe.cpp             # Cache implementation (get/put/clear)
```

## 🔧 Build & Run

### Prerequisites

- **Linux** (uses POSIX sockets)
- **CMake** 3.16.3 or higher
- **GCC** or **Clang** with C++20 support (GCC 10+, Clang 12+)

### Build

```bash
# Create build directory and compile
mkdir -p build && cd build
cmake ..
make

# Run the server
./MTD_server
```

### Using CLion / IDE

Just open the project folder — CMakeLists.txt will be detected automatically.

## 🚀 Available Routes

Once the server is running, visit these URLs in your browser or use `curl`:

| Route | Cached? | Description |
|-------|---------|-------------|
| `http://localhost:8080/` | ✅ Yes | Home page with a welcome message |
| `http://localhost:8080/health` | ✅ Yes | Health check — returns JSON status |
| `http://localhost:8080/slow` | ✅ Yes | Simulates a 2s operation (instant after first hit!) |
| `http://localhost:8080/echo` | ❌ No | Echoes back your request method and path |
| `http://localhost:8080/cache-stats` | ❌ No | Live cache metrics (hits, misses, hit rate) |

### Quick Test with curl

```bash
# Basic test
curl http://localhost:8080/

# See the slow route (first request = 2s, second request = instant)
curl http://localhost:8080/slow
curl http://localhost:8080/slow    # This one is instant!

# Check cache stats
curl http://localhost:8080/cache-stats
```

## ⚙️ Configuration

The server defaults can be changed in `src/main.cpp`:

| Setting | Default | Location |
|---------|---------|----------|
| Port | `8080` | `Server server(8080, 4)` |
| Worker Threads | `4` | `Server server(8080, 4)` |
| Cache Capacity | `100` items | `Server.h` → `LRUCache cache{100,30}` |
| Cache TTL | `30` seconds | `Server.h` → `LRUCache cache{100,30}` |

