/*
 * Mutex Example
 *
 * Demonstrates pollcoro::mutex and pollcoro::shared_mutex for safe
 * synchronization across yield points.
 *
 * Standard mutexes (std::mutex) are unsafe to hold across co_await because
 * a mutex can only be unlocked on the same thread that locked it, but after
 * a yield point, your coroutine might be polled by a different thread.
 *
 * pollcoro provides mutex and shared_mutex that are safe to use in async code.
 */

#include <coroutine>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

import pollcoro;

// =============================================================================
// Example 1: Basic mutex usage
// =============================================================================

pollcoro::mutex counter_mtx;
int shared_counter = 0;

pollcoro::task<> increment_counter(int id, int times) {
    for (int i = 0; i < times; ++i) {
        auto guard = co_await counter_mtx.lock();
        int old_value = shared_counter;
        co_await pollcoro::yield();  // Simulate async work while holding lock
        shared_counter = old_value + 1;
        std::cout << "  Task " << id << " incremented counter to " << shared_counter << std::endl;
        // guard automatically released here
    }
}

pollcoro::task<> test_basic_mutex() {
    std::cout << "=== Basic Mutex ===" << std::endl;
    shared_counter = 0;

    // Run multiple tasks concurrently that all try to increment the counter
    co_await pollcoro::wait_all(
        increment_counter(1, 3), increment_counter(2, 3), increment_counter(3, 3)
    );

    std::cout << "Final counter value: " << shared_counter << std::endl;
    std::cout << "Expected: 9" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Example 2: try_lock for non-blocking acquisition
// =============================================================================

pollcoro::task<> test_try_lock() {
    std::cout << "=== try_lock (non-blocking) ===" << std::endl;

    pollcoro::mutex mtx;

    // First lock should succeed
    if (auto guard = mtx.try_lock()) {
        std::cout << "  First try_lock succeeded" << std::endl;

        // Second try_lock while first is held should fail
        if (auto guard2 = mtx.try_lock()) {
            std::cout << "  Second try_lock succeeded (unexpected!)" << std::endl;
        } else {
            std::cout << "  Second try_lock failed (expected - mutex is held)" << std::endl;
        }
    }

    // After guard goes out of scope, try_lock should succeed again
    if (auto guard = mtx.try_lock()) {
        std::cout << "  Third try_lock succeeded (after release)" << std::endl;
    }

    std::cout << std::endl;

    co_return;
}

// =============================================================================
// Example 3: Early unlock
// =============================================================================

pollcoro::task<> test_early_unlock() {
    std::cout << "=== Early Unlock ===" << std::endl;

    pollcoro::mutex mtx;

    auto guard = co_await mtx.lock();
    std::cout << "  Lock acquired" << std::endl;
    std::cout << "  Doing critical work..." << std::endl;
    co_await pollcoro::yield();

    guard.unlock();  // Release early
    std::cout << "  Lock released early" << std::endl;

    std::cout << "  Doing non-critical work (no lock held)..." << std::endl;
    co_await pollcoro::yield();

    std::cout << std::endl;
}

// =============================================================================
// Example 4: shared_mutex - multiple readers
// =============================================================================

pollcoro::shared_mutex data_mtx;
std::string shared_data = "initial";

pollcoro::task<> reader(int id) {
    auto guard = co_await data_mtx.lock_shared();
    std::cout << "  Reader " << id << " sees: \"" << shared_data << "\"" << std::endl;
    co_await pollcoro::yield();  // Simulate reading
    std::cout << "  Reader " << id << " done" << std::endl;
}

pollcoro::task<> writer(int id, std::string new_value) {
    auto guard = co_await data_mtx.lock();
    std::cout << "  Writer " << id << " updating to: \"" << new_value << "\"" << std::endl;
    co_await pollcoro::yield();  // Simulate writing
    shared_data = std::move(new_value);
    std::cout << "  Writer " << id << " done" << std::endl;
}

pollcoro::task<> test_shared_mutex() {
    std::cout << "=== shared_mutex (readers/writers) ===" << std::endl;
    shared_data = "initial";

    // Multiple readers can run concurrently
    std::cout << "Multiple concurrent readers:" << std::endl;
    co_await pollcoro::wait_all(reader(1), reader(2), reader(3));

    // Writers get exclusive access
    std::cout << "\nWriter with exclusive access:" << std::endl;
    co_await writer(1, "updated by writer 1");

    // More readers see the updated value
    std::cout << "\nReaders after write:" << std::endl;
    co_await pollcoro::wait_all(reader(4), reader(5));

    std::cout << std::endl;
}

// =============================================================================
// Example 5: shared_mutex try_lock variants
// =============================================================================

pollcoro::task<> test_shared_mutex_try_lock() {
    std::cout << "=== shared_mutex try_lock variants ===" << std::endl;

    pollcoro::shared_mutex mtx;

    // Multiple shared locks can be acquired
    std::cout << "Acquiring multiple shared locks:" << std::endl;
    auto shared1 = mtx.try_lock_shared();
    auto shared2 = mtx.try_lock_shared();
    std::cout << "  try_lock_shared #1: " << (shared1 ? "success" : "failed") << std::endl;
    std::cout << "  try_lock_shared #2: " << (shared2 ? "success" : "failed") << std::endl;

    // Exclusive lock fails while shared locks are held
    auto exclusive = mtx.try_lock();
    std::cout << "  try_lock (exclusive) while readers active: "
              << (exclusive ? "success" : "failed") << std::endl;

    // Release shared locks
    shared1.reset();
    shared2.reset();

    // Now exclusive lock should work
    exclusive = mtx.try_lock();
    std::cout << "  try_lock (exclusive) after readers released: "
              << (exclusive ? "success" : "failed") << std::endl;

    // Shared lock fails while exclusive is held
    auto shared3 = mtx.try_lock_shared();
    std::cout << "  try_lock_shared while writer active: " << (shared3 ? "success" : "failed")
              << std::endl;

    std::cout << std::endl;

    co_return;
}

// =============================================================================
// Example 6: Practical example - thread-safe cache
// =============================================================================

struct cache {
    pollcoro::shared_mutex mtx;
    std::unordered_map<std::string, int> data;

    pollcoro::task<std::optional<int>> get(const std::string& key) {
        auto guard = co_await mtx.lock_shared();
        auto it = data.find(key);
        if (it != data.end()) {
            co_return it->second;
        }
        co_return std::nullopt;
    }

    pollcoro::task<> set(const std::string& key, int value) {
        auto guard = co_await mtx.lock();
        data[key] = value;
    }

    pollcoro::task<int> get_or_compute(const std::string& key, int default_value) {
        // First try with read lock
        {
            auto guard = co_await mtx.lock_shared();
            auto it = data.find(key);
            if (it != data.end()) {
                co_return it->second;
            }
        }

        // Not found - need write lock to insert
        auto guard = co_await mtx.lock();
        // Double-check after acquiring write lock
        auto it = data.find(key);
        if (it != data.end()) {
            co_return it->second;
        }
        data[key] = default_value;
        co_return default_value;
    }
};

pollcoro::task<> cache_reader(cache& c, const std::string& key, int id) {
    auto value = co_await c.get(key);
    if (value) {
        std::cout << "  Reader " << id << " got " << key << " = " << *value << std::endl;
    } else {
        std::cout << "  Reader " << id << " found " << key << " not in cache" << std::endl;
    }
}

pollcoro::task<> test_cache() {
    std::cout << "=== Practical Example: Thread-Safe Cache ===" << std::endl;

    cache c;

    // Populate some data
    co_await c.set("a", 1);
    co_await c.set("b", 2);
    co_await c.set("c", 3);
    std::cout << "Cache populated with a=1, b=2, c=3" << std::endl;

    // Multiple concurrent readers
    std::cout << "\nConcurrent reads:" << std::endl;
    co_await pollcoro::wait_all(
        cache_reader(c, "a", 1),
        cache_reader(c, "b", 2),
        cache_reader(c, "c", 3),
        cache_reader(c, "d", 4)  // Not in cache
    );

    // get_or_compute pattern
    std::cout << "\nget_or_compute for missing key:" << std::endl;
    int value = co_await c.get_or_compute("d", 42);
    std::cout << "  d = " << value << " (computed)" << std::endl;

    value = co_await c.get_or_compute("d", 99);
    std::cout << "  d = " << value << " (from cache, not 99)" << std::endl;

    std::cout << std::endl;
}

// =============================================================================
// Example 7: Demonstrating fairness - FIFO ordering
// =============================================================================

pollcoro::task<> waiter(pollcoro::mutex& mtx, int id) {
    std::cout << "  Task " << id << " waiting for lock..." << std::endl;
    auto guard = co_await mtx.lock();
    std::cout << "  Task " << id << " acquired lock" << std::endl;
    co_await pollcoro::yield();
}

pollcoro::task<> test_fairness() {
    std::cout << "=== FIFO Ordering ===" << std::endl;

    pollcoro::mutex mtx;

    // Hold the lock while others queue up
    auto guard = co_await mtx.lock();
    std::cout << "Initial holder has the lock" << std::endl;

    // Start waiters - they'll queue in order
    auto w1 = waiter(mtx, 1);
    auto w2 = waiter(mtx, 2);
    auto w3 = waiter(mtx, 3);

    // Poll each once to get them queued
    pollcoro::waker noop;
    w1.poll(noop);
    w2.poll(noop);
    w3.poll(noop);

    std::cout << "Releasing initial lock..." << std::endl;
    guard.unlock();

    // Now run them to completion - should complete in order 1, 2, 3
    co_await std::move(w1);
    co_await std::move(w2);
    co_await std::move(w3);

    std::cout << "Tasks completed in FIFO order" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "pollcoro Mutex Examples" << std::endl;
    std::cout << "=======================" << std::endl << std::endl;

    pollcoro::block_on(test_basic_mutex());
    pollcoro::block_on(test_try_lock());
    pollcoro::block_on(test_early_unlock());
    pollcoro::block_on(test_shared_mutex());
    pollcoro::block_on(test_shared_mutex_try_lock());
    pollcoro::block_on(test_cache());
    pollcoro::block_on(test_fairness());

    return 0;
}
