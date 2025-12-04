/*
 * Stream Combinators Example
 *
 * Demonstrates the various stream combinators available in pollcoro:
 * - take, skip, take_while, skip_while
 * - map, flatten, chain
 * - zip, enumerate
 * - fold, last, nth
 * - window, sync_iter
 */

#include <iostream>
#include <pollcoro/block_on.hpp>
#include <pollcoro/chain.hpp>
#include <pollcoro/enumerate.hpp>
#include <pollcoro/flatten.hpp>
#include <pollcoro/fold.hpp>
#include <pollcoro/iter.hpp>
#include <pollcoro/last.hpp>
#include <pollcoro/map.hpp>
#include <pollcoro/next.hpp>
#include <pollcoro/nth.hpp>
#include <pollcoro/range.hpp>
#include <pollcoro/repeat.hpp>
#include <pollcoro/skip.hpp>
#include <pollcoro/skip_while.hpp>
#include <pollcoro/stream.hpp>
#include <pollcoro/sync_iter.hpp>
#include <pollcoro/take.hpp>
#include <pollcoro/take_while.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/window.hpp>
#include <pollcoro/yield.hpp>
#include <pollcoro/zip.hpp>

// =============================================================================
// Helper streams for demonstrations
// =============================================================================

// Infinite stream that generates the Fibonacci sequence
pollcoro::stream<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield b;
        auto tmp = a;
        a = b;
        b += tmp;
    }
}

// Async counter that yields 0, 1, 2, ..., count-1
// Uses co_await to simulate async work between yields
pollcoro::stream<int> async_counter(int count) {
    for (int i = 0; i < count; ++i) {
        co_await pollcoro::yield();  // Simulate async work
        co_yield i;
    }
}

// Simple range stream (for yield-from demonstration)
pollcoro::stream<int> make_range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

// Infinite stream that repeats a value (for flatten demo)
pollcoro::stream<int> repeat_n(int value) {
    while (true) {
        co_yield value;
    }
}

// =============================================================================
// Yield-from demonstration
// =============================================================================

// Stream that yields from other streams using yield-from syntax
pollcoro::stream<int> combined_ranges() {
    co_yield 100;  // Single value
    co_yield make_range(0, 3);  // Yield from: 0, 1, 2
    co_yield 200;  // Single value
    co_yield make_range(10, 13);  // Yield from: 10, 11, 12
    co_yield 300;  // Single value
    co_yield pollcoro::iter(std::array{13, 14, 15});  // From iterator
}

// =============================================================================
// Test functions demonstrating each combinator
// =============================================================================

// Demonstrates: take, skip_while (pipe syntax)
pollcoro::task<> test_fibonacci() {
    std::cout << "=== Fibonacci (skip_while < 100, take 10) ===" << std::endl;

    auto s = fibonacci() | pollcoro::skip_while([](auto n) {
                 return n < 100;
             }) |
        pollcoro::take(10);

    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl << std::endl;
}

// Demonstrates: basic async stream consumption
pollcoro::task<> test_async_counter() {
    std::cout << "=== Async Counter (0-4) ===" << std::endl;

    auto s = async_counter(5);
    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl << std::endl;
}

// Demonstrates: yield-from syntax
pollcoro::task<> test_yield_from() {
    std::cout << "=== Yield-From (combined ranges) ===" << std::endl;

    auto s = combined_ranges();
    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
    std::cout << "Expected: 100 0 1 2 200 10 11 12 300 13 14 15" << std::endl << std::endl;
}

// Demonstrates: chain combinator
pollcoro::task<> test_chain() {
    std::cout << "=== Chain (fib(3) + counter(5), doubled) ===" << std::endl;

    auto s = fibonacci() | pollcoro::take(3) | pollcoro::chain(async_counter(5)) |
        pollcoro::map([](auto n) {
                 return n * 2;
             });

    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
    std::cout << "Expected: 2 2 4 0 2 4 6 8" << std::endl << std::endl;
}

// Demonstrates: map + flatten
pollcoro::task<> test_flatten() {
    std::cout << "=== Flatten (repeat n, n times) ===" << std::endl;

    // For each number n, create a stream that repeats n, n times
    // Then flatten all those streams into one
    auto map_op = [](auto n) {
        return repeat_n(n) | pollcoro::take(n);
    };

    auto s = async_counter(5) | pollcoro::map(map_op) | pollcoro::flatten();

    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
    std::cout << "Expected: 1 2 2 3 3 3 4 4 4 4" << std::endl << std::endl;
}

// Demonstrates: zip with enumerate
pollcoro::task<> test_zip() {
    std::cout << "=== Zip (two counters + enumerate) ===" << std::endl;

    auto s = pollcoro::zip(async_counter(5), async_counter(5), pollcoro::enumerate()) |
        pollcoro::take(3);

    while (auto value = co_await pollcoro::next(s)) {
        auto [a, b, index] = *value;
        std::cout << index << ": (" << a << ", " << b << ") ";
    }
    std::cout << std::endl << std::endl;
}

// Demonstrates: last combinator
pollcoro::task<> test_last() {
    std::cout << "=== Last (counter 0-4) ===" << std::endl;

    auto result = co_await pollcoro::last(async_counter(5));
    if (result) {
        std::cout << "Last value: " << *result << std::endl;
    }
    std::cout << "Expected: 4" << std::endl << std::endl;
}

// Demonstrates: nth combinator (single and repeated)
pollcoro::task<> test_nth() {
    std::cout << "=== Nth (get 3rd element) ===" << std::endl;
    {
        auto s = async_counter(5);
        auto third = co_await pollcoro::nth(s, 3);
        if (third) {
            std::cout << "3rd element: " << *third << std::endl;
        }
        std::cout << "Expected: 2" << std::endl;
    }

    std::cout << std::endl << "=== Nth (every 2nd element) ===" << std::endl;
    {
        auto s = async_counter(10);
        std::cout << "Every 2nd: ";
        while (auto value = co_await pollcoro::nth(s, 2)) {
            std::cout << *value << " ";
        }
        std::cout << std::endl;
        std::cout << "Expected: 1 3 5 7 9" << std::endl << std::endl;
    }
}

// Demonstrates: window combinator
pollcoro::task<> test_window() {
    std::cout << "=== Window (groups of 3) ===" << std::endl;

    auto s = async_counter(9) | pollcoro::window<3>();

    while (auto value = co_await pollcoro::next(s)) {
        auto [a, b, c] = *value;
        std::cout << "(" << a << ", " << b << ", " << c << ") ";
    }
    std::cout << std::endl << std::endl;
}

// Demonstrates: fold with early termination
pollcoro::task<> test_fold() {
    std::cout << "=== Fold (sum until >= 50) ===" << std::endl;

    auto sum = co_await pollcoro::fold(async_counter(10000), 0, [](int& accumulator, int n) {
        accumulator += n;
        return accumulator < 50;  // Continue while sum < 50
    });

    std::cout << "Sum: " << sum << std::endl;
    std::cout << "Expected: 55 (0+1+2+...+10)" << std::endl << std::endl;
}

// Demonstrates: range and repeat combinators
pollcoro::task<> test_range_repeat() {
    std::cout << "=== Range (0-4) ===" << std::endl;
    {
        auto s = pollcoro::range(5);
        while (auto value = co_await pollcoro::next(s)) {
            std::cout << *value << " ";
        }
        std::cout << std::endl;
    }

    std::cout << std::endl << "=== Range (5-9) ===" << std::endl;
    {
        auto s = pollcoro::range(5, 10);
        while (auto value = co_await pollcoro::next(s)) {
            std::cout << *value << " ";
        }
        std::cout << std::endl;
    }

    std::cout << std::endl << "=== Repeat (42 x 5) ===" << std::endl;
    {
        auto s = pollcoro::repeat(42) | pollcoro::take(5);
        while (auto value = co_await pollcoro::next(s)) {
            std::cout << *value << " ";
        }
        std::cout << std::endl << std::endl;
    }
}

// Demonstrates: take_while combinator
pollcoro::task<> test_take_while() {
    std::cout << "=== Take While (< 5) ===" << std::endl;

    auto s = pollcoro::range(10) | pollcoro::take_while([](auto n) {
                 return n < 5;
             });

    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
    std::cout << "Expected: 0 1 2 3 4" << std::endl << std::endl;
}

// Demonstrates: iter combinator (from containers)
pollcoro::task<> test_iter() {
    std::cout << "=== Iter (from vector) ===" << std::endl;
    {
        std::vector<int> vec = {10, 20, 30, 40, 50};
        auto s = pollcoro::iter(vec);
        while (auto value = co_await pollcoro::next(s)) {
            std::cout << *value << " ";
        }
        std::cout << std::endl;
    }

    std::cout << std::endl << "=== Iter (from array) ===" << std::endl;
    {
        auto s = pollcoro::iter(std::array{1, 2, 3, 4, 5});
        while (auto value = co_await pollcoro::next(s)) {
            std::cout << *value << " ";
        }
        std::cout << std::endl << std::endl;
    }
}

// Demonstrates: sync_iter for range-based for loops
void test_sync_iter() {
    std::cout << "=== Sync Iter (range-based for) ===" << std::endl;

    auto s = pollcoro::range(5);
    for (auto value : pollcoro::sync_iter(std::move(s))) {
        std::cout << value << " ";
    }
    std::cout << std::endl << std::endl;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "pollcoro Stream Combinators Demo" << std::endl;
    std::cout << "=================================" << std::endl << std::endl;

    pollcoro::block_on(test_fibonacci());
    pollcoro::block_on(test_async_counter());
    pollcoro::block_on(test_yield_from());
    pollcoro::block_on(test_chain());
    pollcoro::block_on(test_flatten());
    pollcoro::block_on(test_zip());
    pollcoro::block_on(test_last());
    pollcoro::block_on(test_nth());
    pollcoro::block_on(test_window());
    pollcoro::block_on(test_fold());
    pollcoro::block_on(test_range_repeat());
    pollcoro::block_on(test_take_while());
    pollcoro::block_on(test_iter());
    test_sync_iter();

    return 0;
}
