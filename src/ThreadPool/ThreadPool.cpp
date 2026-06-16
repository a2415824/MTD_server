/*
 * ============================================================================
 *  ThreadPool.cpp — Implementation of the worker thread pool
 * ============================================================================
 *
 *  This is where the magic of multithreading happens.
 *
 *  Key concepts used here:
 *
 *      mutex            → A lock. Only one thread can hold it at a time.
 *                         Like a bathroom door lock — one person at a time!
 *
 *      unique_lock      → A smart wrapper around mutex. Automatically unlocks
 *                         when it goes out of scope (RAII pattern).
 *
 *      condition_variable → A way for threads to sleep until something
 *                           interesting happens (a new task arrives).
 *                           Way better than busy-waiting in a loop!
 *
 *      atomic<bool>     → A thread-safe boolean. Multiple threads can read/write
 *                         it without a mutex. Used for the "stop" flag.
 *
 * ============================================================================
 */

#include "ThreadPool.h"

/*
 * Constructor — Creates worker threads and starts them immediately.
 *
 *  Each worker thread runs an infinite loop:
 *      1. Lock the queue
 *      2. Sleep until there's a task OR we're shutting down
 *      3. Grab the task from the front of the queue
 *      4. Unlock the queue (so other threads can grab tasks too)
 *      5. Execute the task
 *      6. Repeat
 */
ThreadPool::ThreadPool(size_t numThreads):stop(false){

    for (size_t i=0;i<numThreads;++i) {

        // emplace_back creates a new thread directly in the vector
        // Each thread runs the lambda function below
        workers.emplace_back([this] {
            while (true) {
                std::function<void()>task;

                // ── Critical section start ──
                // Only one thread at a time can access the task queue
                {
                    std::unique_lock<std::mutex> lock(queueMutex);

                    // Wait here until EITHER:
                    //   a) stop is true (server is shutting down), OR
                    //   b) there's a task in the queue
                    // This is much better than a busy loop — the thread
                    // actually sleeps and uses zero CPU while waiting!
                    condition.wait(lock,[this] {
                        return stop || !tasks.empty();
                    });

                    // If we're shutting down AND there are no tasks left, exit
                    if (stop && tasks.empty())return;

                    // Grab the next task from the front of the queue
                    task = std::move(tasks.front());
                    tasks.pop();
                // ── Critical section end ──
                // The lock is released here (unique_lock destructor)
                }

                // Run the task! This happens OUTSIDE the lock,
                // so other threads can grab tasks while this one works
                task();
            }
        });
    }

}


/*
 * Destructor — Cleanly shuts down all worker threads.
 *
 *  1. Set stop = true (tells all workers to finish up)
 *  2. notify_all() wakes up ALL sleeping workers so they can see the stop flag
 *  3. join() waits for each worker to finish its current task before continuing
 *
 *  This ensures no work is lost and no threads are left dangling.
 */
ThreadPool::~ThreadPool() {
    stop = true;

    // Wake up ALL sleeping threads so they can check the stop flag
    condition.notify_all();

    // Wait for each thread to finish — this blocks until the thread exits
    for (std::thread& worker: workers) {
        worker.join();
    }
}

/*
 * enqueue() — Adds a task to the queue and wakes one worker.
 *
 *  This is thread-safe: multiple threads can call enqueue() at the same time
 *  because we protect the queue with a mutex.
 *
 *  notify_one() wakes up exactly ONE sleeping worker thread.
 *  (No point waking all of them — only one task was added!)
 */
void ThreadPool::enqueue(std::function<void()> &&task) {
    {
        // Lock the queue, add the task, then unlock
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.push(std::move(task));
    }

    // Wake up one sleeping worker to handle the new task
    condition.notify_one();
}