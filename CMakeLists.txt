# ============================================================================
#  CMakeLists.txt — Build configuration for MTD_server
# ============================================================================
#
#  This file tells CMake how to compile our project. CMake is a build system
#  generator — it reads this file and creates the actual build instructions
#  (Makefiles on Linux, Visual Studio projects on Windows, etc.)
#
#  To build this project:
#      mkdir build && cd build
#      cmake ..
#      make
#
#  Then run:
#      ./MTD_server
#
# ============================================================================

# Minimum CMake version required (3.16.3 is widely available on Ubuntu 20.04+)
cmake_minimum_required(VERSION 3.16.3)

# Name of our project
project(MTD_server)

# Use C++20 standard — gives us features like std::optional, structured bindings,
# and other modern C++ goodies
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically find ALL .cpp files under src/ (recursively)
file(GLOB_RECURSE SOURCES "src/*.cpp")


# Create the executable from all our source files
add_executable(MTD_server ${SOURCES}
        src/ThreadPool/ThreadPool.cpp
        src/HttpRequest/HttpRequest.h
        src/HttpRequest/HttpRequest.cpp
        src/HttpResponse/HttpResponse.h
        src/HttpResponse/HttpResponse.cpp
        src/Server/Server.h
        src/Server/Server.cpp
        src/LRUCache/LRUCache.h
        src/LRUCache/LRUCahe.cpp)

# Tell the compiler where to find our header files
# PRIVATE means these paths are only used when building this target
target_include_directories(MTD_server PRIVATE
        src/
        src/Server
        src/ThreadPool
        src/HttpRequest
        src/HttpResponse
        src/LRUCache
)

# Link the pthreads library — required for std::thread, std::mutex, etc. on Linux
find_package(Threads REQUIRED)
target_link_libraries(MTD_server Threads::Threads)
