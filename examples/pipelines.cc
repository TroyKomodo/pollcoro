/*
 * Stream Pipelines Example
 *
 * Demonstrates complex stream pipelines and the zero-cost
 * optimization of non-blocking pipelines.
 */

#include <cmath>
#include <coroutine>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

import pollcoro;

// =============================================================================
// Example 1: Zero-cost pipeline (fully non-blocking)
// =============================================================================

void test_zero_cost_pipeline() {
    std::cout << "=== Zero-Cost Pipeline ===" << std::endl;
    std::cout << "This pipeline uses only non-blocking combinators," << std::endl;
    std::cout << "so the compiler can optimize away coroutine overhead." << std::endl;
    std::cout << std::endl;

    // All non-blocking: range, map, take, skip
    // Compiler can optimize this to a simple loop
    auto pipeline = pollcoro::range(1, 100) | pollcoro::map([](int n) {
                        return n * n;
                    }) |
        pollcoro::skip(5) | pollcoro::take(10);

    std::cout << "Squares of 6-15: ";
    for (auto value : pollcoro::sync_iter(std::move(pipeline))) {
        std::cout << value << " ";
    }
    std::cout << std::endl << std::endl;
}

// =============================================================================
// Example 2: Sum of squares using fold
// =============================================================================

void test_fold_pipeline() {
    std::cout << "=== Fold Pipeline (Sum of Squares 1-10) ===" << std::endl;

    auto sum = pollcoro::block_on(
        pollcoro::fold(
            pollcoro::range(1, 11) | pollcoro::map([](int n) {
                return n * n;
            }),
            0,
            [](int& acc, int n) {
                acc += n;
            }
        )
    );

    std::cout << "Sum of squares 1-10: " << sum << std::endl;
    std::cout << "Expected: 385 (1 + 4 + 9 + 16 + 25 + 36 + 49 + 64 + 81 + 100)" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Example 3: Processing with type transformations
// =============================================================================

struct Point {
    int x, y;
};

void test_type_transform_pipeline() {
    std::cout << "=== Type Transform Pipeline ===" << std::endl;

    // Generate points, calculate distances, filter, format as strings
    auto pipeline = pollcoro::range(10) | pollcoro::map([](int i) {
                        return Point{i, i * 2};
                    }) |
        pollcoro::map([](Point p) {
                        return std::sqrt(p.x * p.x + p.y * p.y);
                    }) |
        pollcoro::take_while([](double d) {
                        return d < 15.0;
                    }) |
        pollcoro::map([](double d) {
                        return "dist=" + std::to_string(static_cast<int>(d));
                    });

    std::cout << "Points with distance < 15: ";
    for (auto& s : pollcoro::sync_iter(std::move(pipeline))) {
        std::cout << s << " ";
    }
    std::cout << std::endl << std::endl;
}

// =============================================================================
// Example 4: Moving averages with window
// =============================================================================

pollcoro::task<> test_moving_average() {
    std::cout << "=== Moving Average (Window of 3) ===" << std::endl;

    auto averages =
        pollcoro::range(1, 10) | pollcoro::window<3>() | pollcoro::map([](std::array<int, 3> w) {
            return (w[0] + w[1] + w[2]) / 3.0;
        });

    std::cout << "Input: 1-9, Averages: ";
    while (auto avg = co_await pollcoro::next(averages)) {
        std::cout << *avg << " ";
    }
    std::cout << std::endl << std::endl;
}

// =============================================================================
// Example 5: Indexed processing with zip + enumerate
// =============================================================================

pollcoro::task<> test_indexed_pipeline() {
    std::cout << "=== Indexed Pipeline ===" << std::endl;

    std::vector<std::string> words = {"apple", "banana", "cherry", "date", "elderberry"};

    auto indexed =
        pollcoro::zip(pollcoro::iter(words), pollcoro::enumerate()) | pollcoro::map([](auto pair) {
            auto [word, idx] = pair;
            return std::to_string(idx) + ": " + word;
        });

    while (auto line = co_await pollcoro::next(indexed)) {
        std::cout << *line << std::endl;
    }
    std::cout << std::endl;
}

// =============================================================================
// Example 6: Flattening nested data
// =============================================================================

pollcoro::task<> test_flatten_pipeline() {
    std::cout << "=== Flatten Pipeline ===" << std::endl;

    // Create a stream of streams: [[1], [2,2], [3,3,3], [4,4,4,4]]
    auto nested = pollcoro::range(1, 5) | pollcoro::map([](int n) {
                      return pollcoro::repeat(n) | pollcoro::take(n);
                  }) |
        pollcoro::flatten();

    std::cout << "Flattened: ";
    while (auto v = co_await pollcoro::next(nested)) {
        std::cout << *v << " ";
    }
    std::cout << std::endl;
    std::cout << "Expected: 1 2 2 3 3 3 4 4 4 4" << std::endl << std::endl;
}

// =============================================================================
// Example 7: Chaining multiple sources
// =============================================================================

pollcoro::task<> test_chain_pipeline() {
    std::cout << "=== Chain Pipeline ===" << std::endl;

    auto chained = pollcoro::range(1, 4) |
        pollcoro::chain(pollcoro::repeat(0) | pollcoro::take(2)) |
        pollcoro::chain(pollcoro::range(10, 13));

    std::cout << "Chained [1-3] + [0,0] + [10-12]: ";
    while (auto v = co_await pollcoro::next(chained)) {
        std::cout << *v << " ";
    }
    std::cout << std::endl << std::endl;
}

// =============================================================================
// Example 8: Complex filter with skip_while + take_while
// =============================================================================

void test_filter_pipeline() {
    std::cout << "=== Filter Pipeline (skip_while + take_while) ===" << std::endl;

    // Find numbers between 20 and 50 that are divisible by 7
    auto filtered = pollcoro::range(1, 100) | pollcoro::map([](int n) {
                        return n * 7;
                    })  // Multiples of 7
        | pollcoro::skip_while([](int n) {
                        return n < 20;
                    }) |
        pollcoro::take_while([](int n) {
                        return n <= 50;
                    });

    std::cout << "Multiples of 7 between 20 and 50: ";
    for (auto v : pollcoro::sync_iter(std::move(filtered))) {
        std::cout << v << " ";
    }
    std::cout << std::endl << std::endl;
}

// =============================================================================
// Example 9: Pagination-style processing
// =============================================================================

pollcoro::task<> test_pagination() {
    std::cout << "=== Pagination (skip + take) ===" << std::endl;

    constexpr int page_size = 5;

    for (int page = 0; page < 3; ++page) {
        std::cout << "Page " << page << ": ";

        auto page_items =
            pollcoro::range(100) | pollcoro::skip(page * page_size) | pollcoro::take(page_size);

        while (auto v = co_await pollcoro::next(page_items)) {
            std::cout << *v << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

// =============================================================================
// Example 10: Early termination with fold
// =============================================================================

pollcoro::task<> test_early_termination() {
    std::cout << "=== Early Termination (fold with predicate) ===" << std::endl;

    // Find first number whose factorial exceeds 1000
    auto result = co_await pollcoro::fold(
        pollcoro::range(1, 20),
        std::pair{1, 1},  // {n, factorial}
        [](auto& acc, int n) {
            acc.first = n;
            acc.second *= n;
            return acc.second <= 1000;  // Continue while factorial <= 1000
        }
    );

    std::cout << "First n where n! > 1000: " << result.first << std::endl;
    std::cout << "Factorial: " << result.second << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "pollcoro Stream Pipelines Examples" << std::endl;
    std::cout << "===================================" << std::endl << std::endl;

    test_zero_cost_pipeline();
    test_fold_pipeline();
    test_type_transform_pipeline();
    pollcoro::block_on(test_moving_average());
    pollcoro::block_on(test_indexed_pipeline());
    pollcoro::block_on(test_flatten_pipeline());
    pollcoro::block_on(test_chain_pipeline());
    test_filter_pipeline();
    pollcoro::block_on(test_pagination());
    pollcoro::block_on(test_early_termination());

    return 0;
}
