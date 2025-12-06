/*
 * cppcoro Interop Examples: pollcoro with cppcoro
 *
 * Demonstrates bidirectional interoperability between:
 * - pollcoro (poll-based) and cppcoro (resume-based)
 *
 * Key adapters in <pollcoro/resumable.hpp>:
 * - to_pollable: resume-based -> poll-based (use cppcoro inside pollcoro)
 * - to_resumable: poll-based -> resume-based (use pollcoro inside cppcoro)
 */

// cppcoro is weird and requires we unset this define before including it
#undef linux

#include <chrono>
#include <cppcoro/async_auto_reset_event.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <iostream>
#include <pollcoro/block_on.hpp>
#include <pollcoro/resumable.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/yield.hpp>
#include <thread>

namespace {
// Utility to print with thread id
template<typename... Args>
void log(Args&&... args) {
    std::cout << "[thread " << std::this_thread::get_id() << "] ";
    (std::cout << ... << std::forward<Args>(args)) << std::endl;
}
}  // namespace

// =============================================================================
// Part 1: cppcoro primitives
// =============================================================================

// Async sleep using cppcoro's event mechanism
cppcoro::task<> cppcoro_sleep(std::chrono::milliseconds duration) {
    log("cppcoro_sleep: starting ", duration.count(), "ms sleep");

    auto event = std::make_shared<cppcoro::async_auto_reset_event>();

    std::thread([event, duration] {
        std::this_thread::sleep_for(duration);
        log("cppcoro_sleep: timer fired");
        event->set();
    }).detach();

    co_await *event;
    log("cppcoro_sleep: resumed");
}

// Async operation that returns a value
cppcoro::task<int> cppcoro_compute(int x) {
    log("cppcoro_compute: computing with x=", x);
    co_await cppcoro_sleep(std::chrono::milliseconds(100));
    co_return x * 2;
}

// =============================================================================
// Part 2: pollcoro tasks
// =============================================================================

// Simple pollcoro task demonstrating yield
pollcoro::task<> poll_with_yields(int count) {
    log("poll_with_yields: starting with count=", count);

    for (int i = 0; i < count; ++i) {
        log("poll_with_yields: iteration ", i + 1, "/", count);
        co_await pollcoro::yield();
    }

    log("poll_with_yields: complete");
}

// pollcoro task that uses a cppcoro awaitable via make_pollable
pollcoro::task<int> poll_using_cppcoro(int value) {
    log("poll_using_cppcoro: starting");

    co_await pollcoro::to_pollable(cppcoro_sleep(std::chrono::milliseconds(200)));

    log("poll_using_cppcoro: after cppcoro sleep, returning ", value);
    co_return value;
}

// Chained pollcoro operations using cppcoro
pollcoro::task<int> poll_chain_cppcoro() {
    log("poll_chain_cppcoro: starting");

    auto a = co_await poll_using_cppcoro(10);
    log("poll_chain_cppcoro: got a=", a);

    auto b = co_await poll_using_cppcoro(20);
    log("poll_chain_cppcoro: got b=", b);

    co_return a + b;
}

// =============================================================================
// Example 1: Basic pollcoro with block_on
// =============================================================================

void example_basic_pollcoro() {
    std::cout << "\n=== Example 1: Basic pollcoro with block_on ===\n" << std::endl;

    pollcoro::block_on(poll_with_yields(3));

    std::cout << "\n--- Example 1 complete ---\n";
}

// =============================================================================
// Example 2: pollcoro using cppcoro awaitable (to_pollable)
// =============================================================================

void example_pollcoro_uses_cppcoro() {
    std::cout << "\n=== Example 2: pollcoro using cppcoro awaitable ===\n" << std::endl;

    auto result = pollcoro::block_on(poll_using_cppcoro(42));
    log("Result: ", result);

    std::cout << "\n--- Example 2 complete ---\n";
}

// =============================================================================
// Example 3: cppcoro driving pollcoro task (to_resumable)
// =============================================================================

void example_cppcoro_drives_pollcoro() {
    std::cout << "\n=== Example 3: cppcoro driving pollcoro task ===\n" << std::endl;

    cppcoro::io_service io_service;

    auto cppcoro_main = [&]() -> cppcoro::task<int> {
        cppcoro::io_work_scope work_scope(io_service);

        log("cppcoro_main: awaiting pollcoro task via make_resumable");
        auto result = co_await pollcoro::to_resumable(poll_chain_cppcoro(), io_service);

        log("cppcoro_main: got result=", result);
        co_return result;
    };

    auto drive_io = [&]() -> cppcoro::task<> {
        log("drive_io: processing events");
        io_service.process_events();
        log("drive_io: done");
        co_return;
    };

    auto result = cppcoro::sync_wait(cppcoro::when_all(cppcoro_main(), drive_io()));

    log("Final result: ", std::get<0>(result));

    std::cout << "\n--- Example 3 complete ---\n";
}

// =============================================================================
// Example 4: Error propagation through interop
// =============================================================================

void example_error_handling() {
    std::cout << "\n=== Example 4: Error handling ===\n" << std::endl;

    auto failing_task = []() -> pollcoro::task<int> {
        log("failing_task: about to throw");
        co_await pollcoro::yield();
        throw std::runtime_error("intentional failure");
        co_return 0;  // unreachable
    };

    try {
        pollcoro::block_on(failing_task());
        log("ERROR: should not reach here");
    } catch (const std::runtime_error& e) {
        log("Caught expected exception: ", e.what());
    }

    std::cout << "\n--- Example 4 complete ---\n";
}

// =============================================================================
// Example 5: Chained cppcoro computations in pollcoro
// =============================================================================

void example_chained_cppcoro() {
    std::cout << "\n=== Example 5: Chained cppcoro computations ===\n" << std::endl;

    auto chained_task = []() -> pollcoro::task<int> {
        log("chained_task: starting");

        // Use cppcoro compute which internally sleeps
        int a = co_await pollcoro::to_pollable(cppcoro_compute(5));
        log("chained_task: got a=", a);

        int b = co_await pollcoro::to_pollable(cppcoro_compute(a));
        log("chained_task: got b=", b);

        co_return a + b;  // 10 + 20 = 30
    };

    auto result = pollcoro::block_on(chained_task());
    log("Result: ", result);

    std::cout << "\n--- Example 5 complete ---\n";
}

// =============================================================================
// Example 6: Mixed pollcoro and cppcoro operations
// =============================================================================

void example_mixed_operations() {
    std::cout << "\n=== Example 6: Mixed pollcoro and cppcoro operations ===\n" << std::endl;

    auto mixed_task = []() -> pollcoro::task<int> {
        log("mixed_task: starting");
        int total = 0;

        for (int i = 1; i <= 3; ++i) {
            // pollcoro yield
            co_await pollcoro::yield();
            log("mixed_task: after pollcoro yield ", i);

            // cppcoro sleep
            co_await pollcoro::to_pollable(cppcoro_sleep(std::chrono::milliseconds(50)));
            log("mixed_task: after cppcoro sleep ", i);

            total += i * 10;
        }

        co_return total;  // 10 + 20 + 30 = 60
    };

    auto result = pollcoro::block_on(mixed_task());
    log("Result: ", result);

    std::cout << "\n--- Example 6 complete ---\n";
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "pollcoro <-> cppcoro Interop Examples\n";
    std::cout << "=====================================\n";
    std::cout << "Main thread: " << std::this_thread::get_id() << "\n";
    std::cout << std::string(50, '=') << std::endl;

    try {
        example_basic_pollcoro();
        example_pollcoro_uses_cppcoro();
        example_cppcoro_drives_pollcoro();
        example_error_handling();
        example_chained_cppcoro();
        example_mixed_operations();

        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "All cppcoro interop examples completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
