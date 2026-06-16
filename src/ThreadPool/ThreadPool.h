/*
 * ============================================================================
 *  ThreadPool.h — A pool of worker threads that process tasks concurrently
 * ============================================================================
 *
 *  WHY DO WE NEED THIS?
 *
 *  Imagine you're running a restaurant. If you only have ONE waiter,
 *  every customer has to wait until the previous one is fully served.
 *  That's slow! A thread pool is like hiring multiple waiters upfront —
 *  they all wait for work, and when a new customer arrives, the next
 *  free waiter immediately takes care of them.
 *
 *  In our server:
 *      - Each "customer" is a client connecting to our HTTP server
 *      - Each "waiter" is a worker thread
 *      - The "kitchen orders" are tasks in the task queue
 *
 *  Without a thread pool, we'd either:
 *      1. Handle one client at a time (too slow), or
 *      2. Create a new thread for EVERY client (expensive — threads are heavy)
 *
 *  The thread pool pre-creates a fixed number of threads, and they all
 *  wait for tasks to appear in the queue. When a task arrives, one
 *  thread wakes up and runs it. This is fast AND efficient.
 *
 *  HOW IT WORKS (step by step):
 *
 *      1. Constructor creates N threads. Each thread runs a loop:
 *           → Wait for a task (sleep until notified)
 *           → Grab the task from the queue
 *           → Execute the task
 *           → Go back to waiting
 *
 *      2. enqueue() adds a new task to the queue and wakes ONE thread.
 *
 *      3. Destructor sets stop=true, wakes ALL threads, and waits for
 *         them to finish their current work before shutting down.
 *
 * ============================================================================
 */

#pragma  once


#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>


class ThreadPool {
 public:

    /*
     * Constructor — Creates `thread_num` worker threads.
     * They start running immediately, waiting for tasks.
     */
    explicit ThreadPool(size_t thread_num);

    /*
     * Destructor — Signals all threads to stop, then waits
     * for each one to finish before cleaning up.
     */
    ~ThreadPool();

    /*
     * enqueue() — Adds a new task (any function) to the queue.
     *
     *  The task is moved (not copied) for efficiency.
     *  One sleeping worker thread gets woken up to run it.
     *
     *  Usage:
     *      pool.enqueue([](){ std::cout << "Hello from a worker!\n"; });
     */
    void enqueue(std::function<void()>&& task);

 private:
    std::vector<std::thread> workers;                  // The actual worker threads
    std::queue<std::function<void()>> tasks;            // Pending tasks waiting to be run

    std::mutex queueMutex;                              // Protects the task queue from race conditions
    std::condition_variable condition;                   // Used to wake up sleeping workers
    std::atomic<bool> stop;                              // Flag to tell workers to shut down
};