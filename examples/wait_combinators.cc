/*
 * Wait Combinators Example
 *
 * Demonstrates wait_all and wait_first for concurrent operations.
 */

#include <coroutine>
#include <iostream>
#include <optional>
#include <tuple>
#include <vector>

import pollcoro;

// =============================================================================
// Helper tasks
// =============================================================================

// A task that yields n times before returning a value
pollcoro::task<int> delayed_value(int value, int delay) {
    for (int i = 0; i < delay; ++i) {
        co_await pollcoro::yield();
    }
    co_return value;
}

// A task that yields n times before completing (void)
pollcoro::task<> delayed_void(int delay, const char* name) {
    for (int i = 0; i < delay; ++i) {
        co_await pollcoro::yield();
    }
    std::cout << "  " << name << " completed" << std::endl;
}

// =============================================================================
// Example 1: wait_all with variadic tasks
// =============================================================================

pollcoro::task<> test_wait_all_variadic() {
    std::cout << "=== wait_all (variadic) ===" << std::endl;

    // Wait for all tasks to complete, get all results
    auto [a, b, c] = co_await pollcoro::wait_all(
        delayed_value(10, 3), delayed_value(20, 1), delayed_value(30, 2)
    );

    std::cout << "Results: " << a << ", " << b << ", " << c << std::endl;
    std::cout << "Expected: 10, 20, 30" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Example 2: wait_all with void tasks
// =============================================================================

pollcoro::task<> test_wait_all_void() {
    std::cout << "=== wait_all (void tasks) ===" << std::endl;

    // Wait for all void tasks - they complete in parallel
    co_await pollcoro::wait_all(
        delayed_void(3, "Task A"), delayed_void(1, "Task B"), delayed_void(2, "Task C")
    );

    std::cout << "All tasks completed!" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Example 3: wait_all with mixed types
// =============================================================================

pollcoro::task<> test_wait_all_mixed() {
    std::cout << "=== wait_all (mixed types) ===" << std::endl;

    // Mix of different return types
    auto [num, str] =
        co_await pollcoro::wait_all(delayed_value(42, 2), []() -> pollcoro::task<std::string> {
            co_await pollcoro::yield();
            co_return "hello";
        }());

    std::cout << "Results: " << num << ", " << str << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Example 4: wait_all with vector of tasks
// =============================================================================

pollcoro::task<> test_wait_all_vector() {
    std::cout << "=== wait_all (vector) ===" << std::endl;

    // Create a vector of tasks
    std::vector<pollcoro::task<int>> tasks;
    for (int i = 0; i < 5; ++i) {
        tasks.push_back(delayed_value(i * 10, i));
    }

    // Wait for all and get vector of results
    std::vector<int> results = co_await pollcoro::wait_all(tasks);

    std::cout << "Results: ";
    for (int r : results) {
        std::cout << r << " ";
    }
    std::cout << std::endl;
    std::cout << "Expected: 0 10 20 30 40" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Example 5: wait_first with variadic tasks
// =============================================================================

pollcoro::task<> test_wait_first_variadic() {
    std::cout << "=== wait_first (variadic) ===" << std::endl;

    // Wait for first task to complete
    auto [result, index] = co_await pollcoro::wait_first(
        delayed_value(10, 5),  // Slowest
        delayed_value(20, 1),  // Fastest - should win
        delayed_value(30, 3)
    );

    std::cout << "First result: " << result << " (index " << index << ")" << std::endl;
    std::cout << "Expected: 20 (index 1)" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Example 6: wait_first with vector of tasks
// =============================================================================

pollcoro::task<> test_wait_first_vector() {
    std::cout << "=== wait_first (vector) ===" << std::endl;

    // Create tasks with different delays
    std::vector<pollcoro::task<int>> tasks;
    tasks.push_back(delayed_value(100, 10));
    tasks.push_back(delayed_value(200, 5));
    tasks.push_back(delayed_value(300, 1));  // Fastest
    tasks.push_back(delayed_value(400, 8));

    auto [result, index] = co_await pollcoro::wait_first(tasks);

    std::cout << "First result: " << result << " (index " << index << ")" << std::endl;
    std::cout << "Expected: 300 (index 2)" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Example 7: Practical pattern - timeout simulation
// =============================================================================

pollcoro::task<int> slow_operation() {
    // Simulate a slow operation
    for (int i = 0; i < 100; ++i) {
        co_await pollcoro::yield();
    }
    co_return 42;
}

pollcoro::task<int> timeout_marker(int timeout_polls) {
    for (int i = 0; i < timeout_polls; ++i) {
        co_await pollcoro::yield();
    }
    co_return -1;  // Timeout marker
}

pollcoro::task<> test_timeout_pattern() {
    std::cout << "=== Timeout Pattern ===" << std::endl;

    // Race between operation and timeout
    auto [result, index] = co_await pollcoro::wait_first(
        slow_operation(),
        timeout_marker(10)  // Times out after 10 polls
    );

    if (index == 0) {
        std::cout << "Operation completed with result: " << result << std::endl;
    } else {
        std::cout << "Operation timed out!" << std::endl;
    }
    std::cout << std::endl;
}

// =============================================================================
// Example 8: Parallel computation
// =============================================================================

pollcoro::task<int> compute_partial(int start, int count) {
    int sum = 0;
    for (int i = start; i < start + count; ++i) {
        sum += i;
        co_await pollcoro::yield();  // Simulate work
    }
    co_return sum;
}

pollcoro::task<> test_parallel_computation() {
    std::cout << "=== Parallel Computation ===" << std::endl;

    // Split computation into 4 parallel tasks
    auto [r1, r2, r3, r4] = co_await pollcoro::wait_all(
        compute_partial(0, 25),
        compute_partial(25, 25),
        compute_partial(50, 25),
        compute_partial(75, 25)
    );

    int total = r1 + r2 + r3 + r4;
    std::cout << "Partial sums: " << r1 << " + " << r2 << " + " << r3 << " + " << r4 << std::endl;
    std::cout << "Total (sum 0-99): " << total << std::endl;
    std::cout << "Expected: 4950" << std::endl;
    std::cout << std::endl;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "pollcoro Wait Combinators Examples" << std::endl;
    std::cout << "===================================" << std::endl << std::endl;

    pollcoro::block_on(test_wait_all_variadic());
    pollcoro::block_on(test_wait_all_void());
    pollcoro::block_on(test_wait_all_mixed());
    pollcoro::block_on(test_wait_all_vector());
    pollcoro::block_on(test_wait_first_variadic());
    pollcoro::block_on(test_wait_first_vector());
    pollcoro::block_on(test_timeout_pattern());
    pollcoro::block_on(test_parallel_computation());

    return 0;
}
