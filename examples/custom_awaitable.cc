/*
 * Custom Awaitable Example
 *
 * Demonstrates how to create custom awaitables without using coroutines.
 * Custom awaitables avoid heap allocation and can be more efficient.
 */

#include <chrono>
#include <iostream>
#include <pollcoro/awaitable.hpp>
#include <pollcoro/block_on.hpp>
#include <pollcoro/is_blocking.hpp>
#include <pollcoro/map.hpp>
#include <pollcoro/ready.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/wait_all.hpp>
#include <pollcoro/waker.hpp>

// =============================================================================
// Example 1: Simple non-blocking awaitable
// =============================================================================

// A simple awaitable that immediately returns a computed value.
// No heap allocation, no coroutine frame.
class add_awaitable : public pollcoro::awaitable_never_blocks {
    int result_;

  public:
    add_awaitable(int a, int b) : result_(a + b) {}

    pollcoro::awaitable_state<int> poll(const pollcoro::waker&) {
        return pollcoro::awaitable_state<int>::ready(result_);
    }
};

auto add(int a, int b) {
    return add_awaitable(a, b);
}

// =============================================================================
// Example 2: Stateful awaitable with multiple polls
// =============================================================================

// An awaitable that counts down and yields when done.
// Demonstrates state that persists across polls.
class countdown_awaitable : public pollcoro::awaitable_always_blocks {
    int count_;

  public:
    countdown_awaitable(int start) : count_(start) {}

    pollcoro::awaitable_state<int> poll(const pollcoro::waker& w) {
        if (count_ <= 0) {
            return pollcoro::awaitable_state<int>::ready(0);
        }
        count_--;
        w.wake();  // Request another poll
        return pollcoro::awaitable_state<int>::pending();
    }
};

auto countdown(int start) {
    return countdown_awaitable(start);
}

// =============================================================================
// Example 3: Wrapper awaitable (maybe_blocks)
// =============================================================================

// Wraps another awaitable and doubles its result.
// Uses awaitable_maybe_blocks to inherit blocking behavior.
template<typename Inner>
class double_awaitable : public pollcoro::awaitable_maybe_blocks<Inner> {
    Inner inner_;
    using result_type = pollcoro::awaitable_result_t<Inner>;

  public:
    double_awaitable(Inner inner) : inner_(std::move(inner)) {}

    pollcoro::awaitable_state<result_type> poll(const pollcoro::waker& w) {
        auto state = inner_.poll(w);
        if (state.is_ready()) {
            return pollcoro::awaitable_state<result_type>::ready(state.take_result() * 2);
        }
        return pollcoro::awaitable_state<result_type>::pending();
    }
};

template<typename Inner>
auto double_result(Inner inner) {
    return double_awaitable<Inner>(std::move(inner));
}

// =============================================================================
// Example 4: Retry awaitable
// =============================================================================

// An awaitable that simulates a flaky operation and retries.
class retry_awaitable : public pollcoro::awaitable_always_blocks {
    int attempts_;
    int max_attempts_;
    int succeed_on_;

  public:
    retry_awaitable(int max_attempts, int succeed_on)
        : attempts_(0), max_attempts_(max_attempts), succeed_on_(succeed_on) {}

    pollcoro::awaitable_state<bool> poll(const pollcoro::waker& w) {
        attempts_++;
        std::cout << "  Attempt " << attempts_ << "/" << max_attempts_ << std::endl;

        if (attempts_ >= succeed_on_) {
            std::cout << "  Success!" << std::endl;
            return pollcoro::awaitable_state<bool>::ready(true);
        }

        if (attempts_ >= max_attempts_) {
            std::cout << "  Failed after " << max_attempts_ << " attempts" << std::endl;
            return pollcoro::awaitable_state<bool>::ready(false);
        }

        w.wake();  // Retry
        return pollcoro::awaitable_state<bool>::pending();
    }
};

auto retry_operation(int max_attempts, int succeed_on) {
    return retry_awaitable(max_attempts, succeed_on);
}

// =============================================================================
// Test functions
// =============================================================================

pollcoro::task<> test_simple_awaitable() {
    std::cout << "=== Simple Non-Blocking Awaitable ===" << std::endl;

    // Custom awaitable - no heap allocation
    int result = co_await add(10, 20);
    std::cout << "add(10, 20) = " << result << std::endl;

    // Compare with pollcoro::ready - also no heap allocation
    int ready_result = co_await pollcoro::ready(42);
    std::cout << "ready(42) = " << ready_result << std::endl;

    // Chain with map
    int mapped = co_await (add(5, 5) | pollcoro::map([](int n) {
                               return n * 10;
                           }));
    std::cout << "add(5, 5) | map(*10) = " << mapped << std::endl;

    std::cout << std::endl;
}

pollcoro::task<> test_stateful_awaitable() {
    std::cout << "=== Stateful Awaitable (Countdown) ===" << std::endl;

    int result = co_await countdown(5);
    std::cout << "countdown(5) completed with: " << result << std::endl;

    std::cout << std::endl;
}

pollcoro::task<> test_wrapper_awaitable() {
    std::cout << "=== Wrapper Awaitable (Double) ===" << std::endl;

    // Wrap a non-blocking awaitable
    int result1 = co_await double_result(add(10, 5));
    std::cout << "double(add(10, 5)) = " << result1 << std::endl;

    // Wrap pollcoro::ready
    int result2 = co_await double_result(pollcoro::ready(21));
    std::cout << "double(ready(21)) = " << result2 << std::endl;

    // Check blocking traits at compile time
    static_assert(!pollcoro::is_blocking_v<add_awaitable>, "add should be non-blocking");
    static_assert(
        !pollcoro::is_blocking_v<double_awaitable<add_awaitable>>,
        "double(add) should be non-blocking"
    );
    static_assert(
        pollcoro::is_blocking_v<double_awaitable<countdown_awaitable>>,
        "double(countdown) should be blocking"
    );

    std::cout << "Blocking traits verified at compile time!" << std::endl;
    std::cout << std::endl;
}

pollcoro::task<> test_retry_awaitable() {
    std::cout << "=== Retry Awaitable ===" << std::endl;

    std::cout << "Operation that succeeds on attempt 3:" << std::endl;
    bool success = co_await retry_operation(5, 3);
    std::cout << "Result: " << (success ? "success" : "failure") << std::endl;

    std::cout << std::endl;
}

pollcoro::task<> test_concurrent_custom() {
    std::cout << "=== Concurrent Custom Awaitables ===" << std::endl;

    // Run multiple custom awaitables concurrently
    auto [a, b, c] = co_await pollcoro::wait_all(add(1, 2), add(3, 4), double_result(add(5, 5)));

    std::cout << "wait_all(add(1,2), add(3,4), double(add(5,5))) = ";
    std::cout << a << ", " << b << ", " << c << std::endl;

    std::cout << std::endl;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "pollcoro Custom Awaitable Examples" << std::endl;
    std::cout << "===================================" << std::endl << std::endl;

    pollcoro::block_on(test_simple_awaitable());
    pollcoro::block_on(test_stateful_awaitable());
    pollcoro::block_on(test_wrapper_awaitable());
    pollcoro::block_on(test_retry_awaitable());
    pollcoro::block_on(test_concurrent_custom());

    return 0;
}
