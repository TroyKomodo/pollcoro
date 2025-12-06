/*
 * ASIO Interop Examples: pollcoro with ASIO
 *
 * Demonstrates bidirectional interoperability between:
 * - pollcoro (poll-based) and ASIO (resume-based)
 *
 * Key adapters in <pollcoro/asio.hpp>:
 * - from_asio: ASIO awaitable -> pollcoro awaitable (use ASIO inside pollcoro)
 * - to_asio: pollcoro awaitable -> ASIO async op (use pollcoro inside ASIO)
 */

#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>
#include <chrono>
#include <iostream>
#include <pollcoro/asio.hpp>
#include <pollcoro/block_on.hpp>
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
// Part 1: ASIO primitives
// =============================================================================

// Async sleep using ASIO timer
asio::awaitable<void> asio_sleep(std::chrono::milliseconds duration) {
    log("asio_sleep: starting ", duration.count(), "ms sleep");

    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor, duration);
    co_await timer.async_wait(asio::use_awaitable);

    log("asio_sleep: resumed");
}

// Async operation that returns a value
asio::awaitable<int> asio_compute(int x) {
    log("asio_compute: computing with x=", x);
    co_await asio_sleep(std::chrono::milliseconds(50));
    co_return x * 3;
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

// =============================================================================
// Example 1: ASIO awaitable inside pollcoro (from_asio)
// =============================================================================

void example_pollcoro_uses_asio() {
    std::cout << "\n=== Example 1: pollcoro using ASIO awaitable (from_asio) ===\n" << std::endl;

    asio::io_context ctx;
    auto executor = ctx.get_executor();

    // Create a pollcoro task that uses ASIO timer via from_asio
    auto pollcoro_with_asio = [&]() -> pollcoro::task<int> {
        log("pollcoro_with_asio: starting");

        // Use from_asio to convert an ASIO awaitable to pollcoro
        co_await pollcoro::from_asio<void>(executor, [&]() -> asio::awaitable<void> {
            log("asio lambda: starting timer");
            asio::steady_timer timer(
                co_await asio::this_coro::executor, std::chrono::milliseconds(100)
            );
            co_await timer.async_wait(asio::use_awaitable);
            log("asio lambda: timer done");
        });

        log("pollcoro_with_asio: after ASIO sleep");

        // Also get a value from ASIO
        int value = co_await pollcoro::from_asio<int>(executor, [&]() -> asio::awaitable<int> {
            log("asio compute: starting");
            asio::steady_timer timer(
                co_await asio::this_coro::executor, std::chrono::milliseconds(50)
            );
            co_await timer.async_wait(asio::use_awaitable);
            log("asio compute: returning 21");
            co_return 21;
        });

        log("pollcoro_with_asio: got value ", value, " from ASIO");
        co_return value * 2;
    };

    // Run io_context in background thread while block_on drives pollcoro
    std::thread io_thread([&ctx]() {
        log("io_thread: running io_context");
        auto work = asio::make_work_guard(ctx);
        ctx.run();
        log("io_thread: io_context done");
    });

    auto result = pollcoro::block_on(pollcoro_with_asio());

    ctx.stop();
    io_thread.join();

    log("Result: ", result);  // 21 * 2 = 42

    std::cout << "\n--- Example 1 complete ---\n";
}

// =============================================================================
// Example 2: ASIO coroutine using pollcoro task (to_asio)
// =============================================================================

void example_asio_uses_pollcoro() {
    std::cout << "\n=== Example 2: ASIO coroutine using pollcoro task ===\n" << std::endl;

    asio::io_context ctx;

    auto asio_main = []() -> asio::awaitable<int> {
        auto executor = co_await asio::this_coro::executor;

        log("asio_main: starting");

        // Use a pollcoro task inside ASIO via to_asio
        auto pollcoro_task = poll_with_yields(3);
        co_await pollcoro::to_asio(executor, std::move(pollcoro_task), asio::use_awaitable);

        log("asio_main: pollcoro task complete");
        co_return 100;
    };

    auto future = asio::co_spawn(ctx, asio_main(), asio::use_future);
    ctx.run();

    log("Result: ", future.get());

    std::cout << "\n--- Example 2 complete ---\n";
}

// =============================================================================
// Example 3: Nested interop (ASIO -> pollcoro -> ASIO)
// =============================================================================

void example_nested_interop() {
    std::cout << "\n=== Example 3: Nested interop (ASIO -> pollcoro -> ASIO) ===\n" << std::endl;

    asio::io_context ctx;

    auto asio_main = [&]() -> asio::awaitable<int> {
        auto executor = co_await asio::this_coro::executor;

        log("asio_main: starting nested interop");

        // pollcoro task that uses ASIO internally
        auto pollcoro_task = [&]() -> pollcoro::task<int> {
            log("pollcoro_task: starting");

            // Use ASIO timer inside pollcoro
            int value = co_await pollcoro::from_asio<int>(executor, []() -> asio::awaitable<int> {
                auto exec = co_await asio::this_coro::executor;
                asio::steady_timer timer(exec, std::chrono::milliseconds(100));
                co_await timer.async_wait(asio::use_awaitable);
                co_return 21;
            });

            log("pollcoro_task: got value ", value, " from inner ASIO");
            co_return value;
        };

        // Convert pollcoro -> ASIO awaitable
        int result = co_await pollcoro::to_asio(executor, pollcoro_task(), asio::use_awaitable);

        log("asio_main: got result from nested chain: ", result);
        co_return result * 2;
    };

    auto future = asio::co_spawn(ctx, asio_main(), asio::use_future);
    ctx.run();

    log("Final result: ", future.get());  // 21 * 2 = 42

    std::cout << "\n--- Example 3 complete ---\n";
}

// =============================================================================
// Example 4: ASIO timer with pollcoro computation
// =============================================================================

void example_asio_timer_with_pollcoro() {
    std::cout << "\n=== Example 4: ASIO timer with pollcoro computation ===\n" << std::endl;

    asio::io_context ctx;

    auto asio_main = []() -> asio::awaitable<int> {
        auto executor = co_await asio::this_coro::executor;

        log("asio_main: starting timer sequence");

        int total = 0;

        for (int i = 1; i <= 3; ++i) {
            // ASIO timer
            asio::steady_timer timer(executor, std::chrono::milliseconds(50));
            co_await timer.async_wait(asio::use_awaitable);
            log("asio_main: timer ", i, " fired");

            // pollcoro computation
            auto compute = [i]() -> pollcoro::task<int> {
                log("pollcoro compute: starting for i=", i);
                co_await pollcoro::yield();
                co_return i * 10;
            };

            int value = co_await pollcoro::to_asio(executor, compute(), asio::use_awaitable);
            log("asio_main: pollcoro returned ", value);

            total += value;
        }

        co_return total;
    };

    auto future = asio::co_spawn(ctx, asio_main(), asio::use_future);
    ctx.run();

    log("Total: ", future.get());  // 10 + 20 + 30 = 60

    std::cout << "\n--- Example 4 complete ---\n";
}

// =============================================================================
// Example 5: Returning values from pollcoro in ASIO
// =============================================================================

void example_pollcoro_returning_values() {
    std::cout << "\n=== Example 5: Returning values from pollcoro in ASIO ===\n" << std::endl;

    asio::io_context ctx;

    auto asio_main = []() -> asio::awaitable<int> {
        auto executor = co_await asio::this_coro::executor;

        log("asio_main: calling pollcoro task that returns a value");

        // A pollcoro task that computes and returns a value
        auto pollcoro_task = []() -> pollcoro::task<int> {
            log("pollcoro_task: starting computation");
            int sum = 0;
            for (int i = 1; i <= 5; ++i) {
                log("pollcoro_task: adding ", i);
                sum += i;
                co_await pollcoro::yield();
            }
            log("pollcoro_task: done, sum=", sum);
            co_return sum;
        };

        // Use to_asio to get the result in ASIO
        int result = co_await pollcoro::to_asio(executor, pollcoro_task(), asio::use_awaitable);

        log("asio_main: got result ", result);
        co_return result * 2;
    };

    auto future = asio::co_spawn(ctx, asio_main(), asio::use_future);
    ctx.run();

    log("Result: ", future.get());  // (1+2+3+4+5) * 2 = 30

    std::cout << "\n--- Example 5 complete ---\n";
}

// =============================================================================
// Example 6: Concurrent pollcoro tasks in ASIO
// =============================================================================

void example_concurrent_pollcoro_in_asio() {
    std::cout << "\n=== Example 6: Concurrent pollcoro tasks in ASIO ===\n" << std::endl;

    asio::io_context ctx;

    auto asio_main = []() -> asio::awaitable<void> {
        auto executor = co_await asio::this_coro::executor;

        log("asio_main: spawning concurrent pollcoro tasks");

        // Spawn multiple pollcoro tasks as ASIO operations
        auto task_a = [](int id) -> pollcoro::task<int> {
            log("task_", id, ": starting");
            for (int i = 0; i < 3; ++i) {
                co_await pollcoro::yield();
            }
            log("task_", id, ": done");
            co_return id * 100;
        };

        // Launch tasks concurrently using futures
        auto future_a = asio::co_spawn(
            executor,
            [&]() -> asio::awaitable<int> {
                co_return co_await pollcoro::to_asio(
                    co_await asio::this_coro::executor, task_a(1), asio::use_awaitable
                );
            },
            asio::use_future
        );

        auto future_b = asio::co_spawn(
            executor,
            [&]() -> asio::awaitable<int> {
                co_return co_await pollcoro::to_asio(
                    co_await asio::this_coro::executor, task_a(2), asio::use_awaitable
                );
            },
            asio::use_future
        );

        auto future_c = asio::co_spawn(
            executor,
            [&]() -> asio::awaitable<int> {
                co_return co_await pollcoro::to_asio(
                    co_await asio::this_coro::executor, task_a(3), asio::use_awaitable
                );
            },
            asio::use_future
        );

        // Wait for all via small sleep (in real code you'd use proper synchronization)
        asio::steady_timer timer(executor, std::chrono::milliseconds(100));
        co_await timer.async_wait(asio::use_awaitable);

        log("asio_main: results = ", future_a.get(), ", ", future_b.get(), ", ", future_c.get());
    };

    auto future = asio::co_spawn(ctx, asio_main(), asio::use_future);
    ctx.run();
    future.get();

    std::cout << "\n--- Example 6 complete ---\n";
}

// =============================================================================
// Example 7: Multiple ASIO operations in pollcoro (from_asio chaining)
// =============================================================================

void example_from_asio_chaining() {
    std::cout << "\n=== Example 7: Chaining ASIO operations in pollcoro ===\n" << std::endl;

    asio::io_context ctx;
    auto executor = ctx.get_executor();

    // A pollcoro task that chains multiple ASIO operations
    auto pollcoro_chain = [&]() -> pollcoro::task<int> {
        log("pollcoro_chain: starting chain of ASIO operations");

        int total = 0;

        for (int i = 1; i <= 3; ++i) {
            // Each iteration uses ASIO for async work
            int value = co_await pollcoro::from_asio<int>(executor, [i]() -> asio::awaitable<int> {
                auto exec = co_await asio::this_coro::executor;
                log("asio step ", i, ": waiting");
                asio::steady_timer timer(exec, std::chrono::milliseconds(30));
                co_await timer.async_wait(asio::use_awaitable);
                log("asio step ", i, ": done");
                co_return i * 10;
            });

            log("pollcoro_chain: step ", i, " returned ", value);
            total += value;

            // Can mix with pollcoro operations
            co_await pollcoro::yield();
        }

        log("pollcoro_chain: total = ", total);
        co_return total;
    };

    // Run io_context in background
    std::thread io_thread([&ctx]() {
        auto work = asio::make_work_guard(ctx);
        ctx.run();
    });

    auto result = pollcoro::block_on(pollcoro_chain());

    ctx.stop();
    io_thread.join();

    log("Result: ", result);  // 10 + 20 + 30 = 60

    std::cout << "\n--- Example 7 complete ---\n";
}

// =============================================================================
// Example 8: Full bidirectional (ASIO -> pollcoro -> ASIO -> pollcoro)
// =============================================================================

void example_full_bidirectional() {
    std::cout << "\n=== Example 8: Full bidirectional interop ===\n" << std::endl;

    asio::io_context ctx;

    // ASIO coroutine that:
    // 1. Calls pollcoro task (via to_asio)
    // 2. Which calls ASIO timer (via from_asio)
    // 3. Which does more pollcoro work
    auto asio_main = [&]() -> asio::awaitable<int> {
        auto executor = co_await asio::this_coro::executor;
        log("asio_main: starting full bidirectional example");

        // Step 1: Call a pollcoro task
        auto pollcoro_outer = [&]() -> pollcoro::task<int> {
            log("pollcoro_outer: starting");

            // Step 2: Use ASIO from within pollcoro
            int asio_result =
                co_await pollcoro::from_asio<int>(executor, []() -> asio::awaitable<int> {
                    log("inner_asio: doing async work");
                    auto exec = co_await asio::this_coro::executor;
                    asio::steady_timer timer(exec, std::chrono::milliseconds(50));
                    co_await timer.async_wait(asio::use_awaitable);
                    log("inner_asio: done");
                    co_return 100;
                });

            log("pollcoro_outer: got ", asio_result, " from ASIO");

            // Step 3: More pollcoro work
            co_await pollcoro::yield();
            log("pollcoro_outer: after yield");

            co_return asio_result + 11;
        };

        int result = co_await pollcoro::to_asio(executor, pollcoro_outer(), asio::use_awaitable);

        log("asio_main: final result = ", result);
        co_return result;
    };

    auto future = asio::co_spawn(ctx, asio_main(), asio::use_future);
    ctx.run();

    log("Result: ", future.get());  // 100 + 11 = 111

    std::cout << "\n--- Example 8 complete ---\n";
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "pollcoro <-> ASIO Interop Examples\n";
    std::cout << "==================================\n";
    std::cout << "Main thread: " << std::this_thread::get_id() << "\n";
    std::cout << std::string(50, '=') << std::endl;

    try {
        example_pollcoro_uses_asio();
        example_asio_uses_pollcoro();
        example_nested_interop();
        example_asio_timer_with_pollcoro();
        example_pollcoro_returning_values();
        example_concurrent_pollcoro_in_asio();
        example_from_asio_chaining();
        example_full_bidirectional();

        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "All ASIO interop examples completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
