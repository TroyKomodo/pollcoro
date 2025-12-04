/*
 * Generic Awaitable Example
 *
 * Demonstrates type-erased awaitables and streams using
 * generic_awaitable<T> and generic_stream_awaitable<T>.
 */

#include <iostream>
#include <pollcoro/block_on.hpp>
#include <pollcoro/generic.hpp>
#include <pollcoro/map.hpp>
#include <pollcoro/next.hpp>
#include <pollcoro/range.hpp>
#include <pollcoro/ready.hpp>
#include <pollcoro/repeat.hpp>
#include <pollcoro/stream.hpp>
#include <pollcoro/take.hpp>
#include <pollcoro/task.hpp>
#include <vector>

// =============================================================================
// Example 1: Generic awaitables for heterogeneous storage
// =============================================================================

pollcoro::task<int> compute_task(int x) {
    co_return x * 2;
}

pollcoro::task<> test_generic_awaitable() {
    std::cout << "=== Generic Awaitable (Type Erasure) ===" << std::endl;

    // Store different awaitable types in the same container
    std::vector<pollcoro::generic_awaitable<int>> awaitables;

    // Add a task
    awaitables.push_back(pollcoro::generic(compute_task(10)));

    // Add a ready value
    awaitables.push_back(pollcoro::generic(pollcoro::ready(42)));

    // Add a mapped ready
    awaitables.push_back(pollcoro::generic(pollcoro::ready(5) | pollcoro::map([](int n) {
                                               return n * 10;
                                           })));

    // Await all of them
    std::cout << "Results: ";
    for (auto& awaitable : awaitables) {
        int result = co_await std::move(awaitable);
        std::cout << result << " ";
    }
    std::cout << std::endl;
    std::cout << "Expected: 20 42 50" << std::endl;

    std::cout << std::endl;
}

// =============================================================================
// Example 2: Generic streams for heterogeneous stream storage
// =============================================================================

pollcoro::stream<int> counter_stream(int count) {
    for (int i = 0; i < count; ++i) {
        co_yield i;
    }
}

pollcoro::task<> test_generic_stream() {
    std::cout << "=== Generic Stream (Type Erasure) ===" << std::endl;

    // Store different stream types in the same container
    std::vector<pollcoro::generic_stream_awaitable<int>> streams;

    // Add a coroutine stream
    streams.push_back(pollcoro::generic(counter_stream(3)));

    // Add a range stream
    streams.push_back(pollcoro::generic(pollcoro::range(10, 13)));

    // Add a repeat stream (with take)
    streams.push_back(pollcoro::generic(pollcoro::repeat(99) | pollcoro::take(2)));

    // Add a mapped range
    streams.push_back(pollcoro::generic(pollcoro::range(3) | pollcoro::map([](int n) {
                                            return n * 100;
                                        })));

    // Consume all streams
    int stream_num = 1;
    for (auto& stream : streams) {
        std::cout << "Stream " << stream_num++ << ": ";
        while (auto value = co_await pollcoro::next(stream)) {
            std::cout << *value << " ";
        }
        std::cout << std::endl;
    }

    std::cout << std::endl;
}

// =============================================================================
// Example 3: Factory function returning different stream types
// =============================================================================

enum class StreamType {
    Counter,
    Range,
    Repeat
};

pollcoro::generic_stream_awaitable<int> create_stream(StreamType type, int param) {
    switch (type) {
    case StreamType::Counter:
        return pollcoro::generic(counter_stream(param));
    case StreamType::Range:
        return pollcoro::generic(pollcoro::range(param));
    case StreamType::Repeat:
        return pollcoro::generic(pollcoro::repeat(param) | pollcoro::take(5));
    }
    // Unreachable, but needed for some compilers
    return pollcoro::generic(pollcoro::range(0));
}

pollcoro::task<> test_factory_function() {
    std::cout << "=== Factory Function with Generic Streams ===" << std::endl;

    auto stream1 = create_stream(StreamType::Counter, 4);
    auto stream2 = create_stream(StreamType::Range, 3);
    auto stream3 = create_stream(StreamType::Repeat, 7);

    std::cout << "Counter(4): ";
    while (auto v = co_await pollcoro::next(stream1)) {
        std::cout << *v << " ";
    }
    std::cout << std::endl;

    std::cout << "Range(3):   ";
    while (auto v = co_await pollcoro::next(stream2)) {
        std::cout << *v << " ";
    }
    std::cout << std::endl;

    std::cout << "Repeat(7):  ";
    while (auto v = co_await pollcoro::next(stream3)) {
        std::cout << *v << " ";
    }
    std::cout << std::endl;

    std::cout << std::endl;
}

// =============================================================================
// Example 4: Runtime stream selection
// =============================================================================

pollcoro::task<> test_runtime_selection() {
    std::cout << "=== Runtime Stream Selection ===" << std::endl;

    for (int i = 0; i < 3; ++i) {
        // Select stream type at runtime
        auto stream = (i % 2 == 0)
            ? pollcoro::generic(pollcoro::range(i + 1, i + 4))
            : pollcoro::generic(pollcoro::repeat(i * 10) | pollcoro::take(3));

        std::cout << "Iteration " << i << ": ";
        while (auto v = co_await pollcoro::next(stream)) {
            std::cout << *v << " ";
        }
        std::cout << std::endl;
    }

    std::cout << std::endl;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "pollcoro Generic Awaitable Examples" << std::endl;
    std::cout << "====================================" << std::endl << std::endl;

    pollcoro::block_on(test_generic_awaitable());
    pollcoro::block_on(test_generic_stream());
    pollcoro::block_on(test_factory_function());
    pollcoro::block_on(test_runtime_selection());

    return 0;
}
