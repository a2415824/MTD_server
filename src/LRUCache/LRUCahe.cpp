/*
 * ============================================================================
 *  LRUCahe.cpp — Implementation of the LRU Cache with TTL
 * ============================================================================
 *
 *  This file implements the get/put/clear operations using the doubly-linked
 *  list + hash map combo described in LRUCache.h.
 *
 *  The key trick is std::list::splice() — it moves a node from one position
 *  in the list to another in O(1) time. When we access an item, we splice
 *  it to the front, making it the "most recently used". The item at the
 *  back is always the "least recently used" and gets evicted first.
 *
 * ============================================================================
 */

#include "LRUCache.h"

/*
 * Constructor — Saves capacity and TTL settings.
 *  The actual list and map start empty — items are added via put().
 */
LRUCache::LRUCache(size_t capacity,int ttlSeconds):cap(capacity),ttlSeconds(ttlSeconds){}


/*
 * isExpired() — Checks if too much time has passed since this entry was cached.
 *
 *  If ttlSeconds is 0, entries never expire (useful for testing).
 *  Otherwise, we compare "now" with "when it was cached" and check
 *  if the difference exceeds our TTL threshold.
 */
bool LRUCache::isExpired(const CacheEntry& entry)const {
    if (ttlSeconds == 0)return false;   // TTL of 0 means "live forever"

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now-entry.cachedAt).count();
    return elapsed>=ttlSeconds;         // true = this entry is stale, remove it
}

/*
 * get() — Looks up a key and returns its value (if found and fresh).
 *
 *  Step by step:
 *      1. Lock the mutex (thread safety — multiple clients may hit the cache)
 *      2. Search the hash map for the key
 *      3. If not found → cache MISS → return empty
 *      4. If found but expired → remove it → cache MISS → return empty
 *      5. If found and fresh → move to front → cache HIT → return the value
 */
std::optional<std::string> LRUCache::get(const std::string &key) {
    std::unique_lock<std::mutex> lock(mutex);

    // Step 2: Look up the key in our hash map
    auto it = map.find(key);

    // Step 3: Key not found — it was never cached or was evicted
    if (it == map.end()) {
        cacheMisses++;
        return std::nullopt;   // std::nullopt means "no value" (like null)
    }

    // Step 4: Key found, but check if it's expired
    if (isExpired(it->second->second)) {
        lruList.erase(it->second);   // Remove from the linked list
        map.erase(it);               // Remove from the hash map
        cacheMisses++;
        return std::nullopt;         // Treat expired entries as if they don't exist
    }

    // Step 5: Cache HIT! Move this item to the front (it's now "most recently used")
    // splice() moves the node WITHOUT copying — just pointer manipulation, O(1)
    lruList.splice(lruList.begin(),lruList,it->second);
    cacheHits++;
    return it->second->second.value;   // Return the cached response body
}

/*
 * put() — Stores a key-value pair in the cache.
 *
 *  Two cases:
 *      a) Key already exists → update the value and move to front
 *      b) Key is new:
 *          → If cache is full, evict the least recently used item (back of list)
 *          → Add the new item to the front of the list
 */
void LRUCache::put(const std::string& key,const std::string& value) {
    std::unique_lock<std::mutex> lock(mutex);

    auto it = map.find(key);

    // Case a) Key already exists — just update it
    if (it!=map.end()) {
        it->second->second.value=value;                                // Update the cached value
        it->second->second.cachedAt=std::chrono::steady_clock::now();  // Reset the TTL timer
        lruList.splice(lruList.begin(),lruList,it->second);            // Move to front
        return;
    }

    // Case b) Key is new — check if we need to evict first
    if (map.size()>=cap) {
        // Cache is full! Remove the LAST item (least recently used)
        auto& lruItem = lruList.back();    // The item at the back = least recently used
        map.erase(lruItem.first);          // Remove its hash map entry
        lruList.pop_back();                // Remove it from the list
    }

    // Create the new cache entry with current timestamp
    CacheEntry entry;
    entry.value = value;
    entry.cachedAt = std::chrono::steady_clock::now();

    // Add to the FRONT of the list (most recently used position)
    lruList.emplace_front(key,entry);

    // Add to the hash map: key → iterator to its position in the list
    map[key] = lruList.begin();
}

/*
 * clear() — Wipes the entire cache.
 *  Thread-safe: acquires the mutex before clearing.
 */
void LRUCache::clear() {
    std::unique_lock<std::mutex> lock(mutex);
    lruList.clear();
    map.clear();
}