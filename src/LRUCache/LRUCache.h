/*
 * ============================================================================
 *  LRUCache.h — Least Recently Used Cache with Time-To-Live (TTL)
 * ============================================================================
 *
 *  WHAT IS AN LRU CACHE?
 *
 *  Imagine you have a small desk that can only hold 5 books. Every time
 *  you read a book, you put it on top of the stack. When you need to add
 *  a new book but the desk is full, you throw away the one at the BOTTOM
 *  (the one you haven't touched in the longest time). That's LRU!
 *
 *      LRU = "Least Recently Used" — the item you haven't used for the
 *            longest time gets removed first when the cache is full.
 *
 *  WHY DO WE USE IT IN OUR SERVER?
 *
 *  Building an HTTP response can be expensive (database calls, file reads,
 *  heavy computation). If someone requests "/slow" and it takes 2 seconds
 *  to generate, we don't want to redo that work for every single request.
 *  Instead, we cache the response the first time and serve it instantly
 *  for future requests.
 *
 *  TTL (Time-To-Live):
 *      Cached items don't stay forever. After `ttlSeconds`, they expire
 *      and the next request will regenerate a fresh response. This ensures
 *      our server doesn't serve stale (outdated) data forever.
 *
 *  DATA STRUCTURE USED:
 *
 *      - std::list (doubly-linked list):
 *          Stores the actual cache entries in order of usage.
 *          Most recently used at the FRONT, least recently used at the BACK.
 *          Moving an item to the front is O(1) — that's the key trick!
 *
 *      - std::unordered_map (hash map):
 *          Maps each key to its position in the list for O(1) lookup.
 *          Without this, finding an item would require scanning the whole list.
 *
 *  Together, they give us O(1) get and O(1) put — the best of both worlds!
 *
 * ============================================================================
 */

#pragma once
#include<string>
#include<list>
#include<unordered_map>
#include<mutex>
#include<atomic>
#include<optional>
#include<chrono>

/*
 * CacheEntry — One item stored in the cache.
 *
 *  value    : the cached response body (a string)
 *  cachedAt : timestamp of when this entry was stored (used for TTL checks)
 */
struct CacheEntry {
    std::string value;
    std::chrono::steady_clock::time_point cachedAt;
};

class LRUCache {
public:

    /*
     * Constructor
     *   capacity    : maximum number of items the cache can hold
     *   ttlSeconds  : how long each item lives before expiring (0 = never expire)
     */
    explicit LRUCache(size_t capacity,int ttlSeconds = 60);

    /*
     * get() — Looks up a key in the cache.
     *
     *   Returns the cached value if found and not expired.
     *   Returns std::nullopt (empty) if the key is missing or expired.
     *   Also moves the item to the front (marks it as "recently used").
     */
    std::optional<std::string>get(const std::string& key);

    /*
     * put() — Stores a key-value pair in the cache.
     *
     *   If the key already exists, updates it and moves it to the front.
     *   If the cache is full, removes the least recently used item first.
     */
    void put(const std::string& key,const std::string& value);

    // --- Stats — useful for monitoring cache performance ---
    size_t hits() const{return cacheHits;}       // How many times we found what we were looking for
    size_t misses() const {return cacheMisses;}  // How many times we had to generate a fresh response
    size_t size() const {return map.size();}     // Current number of items in the cache
    size_t capacity() const{return cap;}         // Maximum number of items we can hold

    /*
     * clear() — Removes ALL items from the cache.
     *  Useful for testing or when you want to force fresh responses.
     */
    void clear();


private:

    size_t cap;         // Maximum capacity of the cache
    int ttlSeconds;     // Time-to-live for each entry (in seconds)

    // The doubly-linked list: each item is a pair of (key, CacheEntry)
    // Front = most recently used, Back = least recently used
    using ListItem = std::pair<std::string,CacheEntry>;
    std::list<ListItem> lruList;

    // Hash map: key → iterator pointing to the item's position in the list
    // This gives us O(1) lookup by key
    std::unordered_map<std::string,std::list<ListItem>::iterator> map;

    // Mutex for thread safety — multiple threads may access the cache at once
    mutable std::mutex mutex;

    // Atomic counters — thread-safe without needing the mutex
    std::atomic<size_t> cacheHits{0};
    std::atomic<size_t> cacheMisses{0};

    /*
     * isExpired() — Checks if a cache entry has lived past its TTL.
     *  Returns true if the entry should be treated as stale.
     */
    bool isExpired(const CacheEntry& entry) const;
};