/*
 * ASIO Interop Examples: pollcoro with ASIO
 *
 * Demonstrates bidirectional interoperability between:
 * - pollcoro (poll-based) and ASIO (resume-based)
 *
 * Key adapters:
 * - from_asio: ASIO awaitable -> pollcoro awaitable (use ASIO inside pollcoro)
 * - to_asio: pollcoro awaitable -> ASIO async op (use pollcoro inside ASIO)
 */

#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>
#include <chrono>
#include <iostream>
#include <thread>

import pollcoro;

namespace {

namespace detail {

template<typename ResultType>
struct poll_signature {
    using type = void(std::exception_ptr, ResultType);
};

template<>
struct poll_signature<void> {
    using type = void(std::exception_ptr);
};

template<typename ResultType>
using poll_signature_t = typename poll_signature<ResultType>::type;

// Shared state for from_asio adapter
template<typename T>
struct asio_shared_state {
    using storage = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

    std::mutex mutex;
    bool started = false;
    bool done = false;
    pollcoro::waker stored_waker;
    std::variant<std::monostate, storage, std::exception_ptr> result;

    void unset_waker() {
        std::lock_guard lock(mutex);
        stored_waker = pollcoro::waker();
    }

    void set_result(storage value) {
        std::lock_guard lock(mutex);
        if constexpr (!std::is_void_v<T>) {
            result.template emplace<storage>(std::move(value));
        }
        done = true;
        stored_waker.wake();
    }

    void set_void_result() {
        std::lock_guard lock(mutex);
        done = true;
        stored_waker.wake();
    }

    void set_exception(std::exception_ptr ex) {
        std::lock_guard lock(mutex);
        result.template emplace<std::exception_ptr>(std::move(ex));
        done = true;
        stored_waker.wake();
    }

    pollcoro::awaitable_state<T> poll(const pollcoro::waker& w) {
        std::lock_guard lock(mutex);
        if (done) {
            if (std::holds_alternative<std::exception_ptr>(result)) {
                std::rethrow_exception(std::get<std::exception_ptr>(result));
            }
            if constexpr (std::is_void_v<T>) {
                return pollcoro::awaitable_state<T>::ready();
            } else {
                return pollcoro::awaitable_state<T>::ready(std::move(std::get<storage>(result)));
            }
        }
        stored_waker = w;
        return pollcoro::awaitable_state<T>::pending();
    }
};

// =============================================================================
// from_asio: Convert an ASIO awaitable to a pollcoro awaitable
// =============================================================================

/// Adapter that wraps an ASIO awaitable factory for use in pollcoro coroutines.
/// The ASIO operation is spawned on the provided executor and runs independently.
/// Poll returns ready when the ASIO operation completes.
template<typename Factory, typename T = std::invoke_result_t<Factory>::value_type>
class from_asio_awaitable : public pollcoro::awaitable_always_blocks {
    using state_type = detail::asio_shared_state<T>;
    std::shared_ptr<state_type> state_;
    asio::any_io_executor executor_;
    std::decay_t<Factory> factory_;

  public:
    from_asio_awaitable(asio::any_io_executor executor, Factory&& factory)
        : state_(std::make_shared<state_type>()),
          executor_(std::move(executor)),
          factory_(std::forward<Factory>(factory)) {}

    from_asio_awaitable(const from_asio_awaitable&) = delete;
    from_asio_awaitable& operator=(const from_asio_awaitable&) = delete;
    from_asio_awaitable(from_asio_awaitable&&) = default;
    from_asio_awaitable& operator=(from_asio_awaitable&&) = default;

    ~from_asio_awaitable() {
        if (state_) {
            state_->unset_waker();
        }
    }

    pollcoro::awaitable_state<T> poll(const pollcoro::waker& w) {
        // Start the ASIO operation on first poll
        if (!state_->started) {
            state_->started = true;
            start_async_operation();
        }
        return state_->poll(w);
    }

  private:
    void start_async_operation() {
        auto state = state_;
        auto factory = std::move(factory_);

        if constexpr (std::is_void_v<T>) {
            asio::co_spawn(
                executor_,
                [factory = std::move(factory)]() -> asio::awaitable<void> {
                    co_await std::invoke(factory);
                },
                [state](std::exception_ptr ex) {
                    if (ex) {
                        state->set_exception(ex);
                    } else {
                        state->set_void_result();
                    }
                }
            );
        } else {
            asio::co_spawn(
                executor_,
                [factory = std::move(factory)]() -> asio::awaitable<T> {
                    co_return co_await std::invoke(factory);
                },
                [state](std::exception_ptr ex, T value) {
                    if (ex) {
                        state->set_exception(ex);
                    } else {
                        state->set_result(std::move(value));
                    }
                }
            );
        }
    }
};

}  // namespace detail

/// Convert an ASIO awaitable factory to a pollcoro awaitable.
/// The factory is a callable that returns an asio::awaitable<T>.
///
/// Example:
///   auto executor = ctx.get_executor();
///   auto pollable = from_asio<int>(executor, [&]() -> asio::awaitable<int> {
///       asio::steady_timer timer(co_await asio::this_coro::executor,
///                                std::chrono::milliseconds(100));
///       co_await timer.async_wait(asio::use_awaitable);
///       co_return 42;
///   });
///   int result = co_await pollable;  // in a pollcoro task
template<typename Factory>
auto from_asio(asio::any_io_executor executor, Factory&& factory) {
    return detail::from_asio_awaitable(std::move(executor), std::forward<Factory>(factory));
}

/// Convenience overload that takes an io_context reference
template<typename Factory>
auto from_asio(asio::io_context& ctx, Factory&& factory) {
    return detail::from_asio_awaitable(ctx.get_executor(), std::forward<Factory>(factory));
}

// =============================================================================
// to_asio: Convert a pollcoro awaitable to an ASIO async operation
// =============================================================================

/// Convert a pollcoro awaitable to an ASIO async operation.
/// Can be used with any ASIO completion token (use_awaitable, use_future, etc.)
///
/// Example:
///   asio::awaitable<int> my_coro() {
///       auto executor = co_await asio::this_coro::executor;
///       co_return co_await to_asio(
///           executor, my_pollcoro_task(), asio::use_awaitable
///       );
///   }
template<pollcoro::awaitable Awaitable, typename CompletionToken>
auto to_asio(asio::any_io_executor executor, Awaitable&& aw, CompletionToken&& token) {
    using result_type = pollcoro::awaitable_result_t<std::decay_t<Awaitable>>;
    using signature = detail::poll_signature_t<result_type>;

    return asio::async_initiate<CompletionToken, signature>(
        [executor](auto handler, std::decay_t<Awaitable> awaitable) mutable {
            struct op_state {
                std::decay_t<Awaitable> awaitable;
                asio::any_io_executor executor;
                std::decay_t<decltype(handler)> handler_;
                std::atomic<bool> wake_pending{true};
                bool polling{false};

                op_state(
                    std::decay_t<Awaitable> aw,
                    asio::any_io_executor exec,
                    std::decay_t<decltype(handler)> h
                )
                    : awaitable(std::move(aw)), executor(std::move(exec)), handler_(std::move(h)) {}

                void poll() {
                    while (wake_pending.exchange(false, std::memory_order_release)) {
                        polling = true;
                        try {
                            auto state = awaitable.poll(pollcoro::waker(this));
                            polling = false;

                            if (state.is_ready()) {
                                complete(std::move(state));
                                return;
                            }
                        } catch (...) {
                            polling = false;
                            complete_with_exception(std::current_exception());
                            return;
                        }
                    }
                }

                void wake() {
                    wake_pending.store(true, std::memory_order_relaxed);
                    if (polling) {
                        return;
                    }
                    asio::post(executor, [this]() {
                        poll();
                    });
                }

                void complete(pollcoro::awaitable_state<result_type> state) {
                    if constexpr (std::is_void_v<result_type>) {
                        std::invoke(std::move(handler_), std::exception_ptr{});
                    } else {
                        try {
                            std::invoke(
                                std::move(handler_), std::exception_ptr{}, state.take_result()
                            );
                        } catch (...) {
                            std::invoke(
                                std::move(handler_), std::current_exception(), result_type{}
                            );
                        }
                    }
                    delete this;
                }

                void complete_with_exception(std::exception_ptr ex) {
                    if constexpr (std::is_void_v<result_type>) {
                        std::invoke(std::move(handler_), ex);
                    } else {
                        std::invoke(std::move(handler_), ex, result_type{});
                    }
                    delete this;
                }
            };

            auto* op = new op_state(std::move(awaitable), executor, std::move(handler));
            op->poll();
        },
        std::forward<CompletionToken>(token),
        std::forward<Awaitable>(aw)
    );
}

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
        co_await from_asio(executor, [&]() -> asio::awaitable<void> {
            log("asio lambda: starting timer");
            asio::steady_timer timer(
                co_await asio::this_coro::executor, std::chrono::milliseconds(100)
            );
            co_await timer.async_wait(asio::use_awaitable);
            log("asio lambda: timer done");
        });

        log("pollcoro_with_asio: after ASIO sleep");

        // Also get a value from ASIO
        int value = co_await from_asio(executor, [&]() -> asio::awaitable<int> {
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
        co_await to_asio(executor, std::move(pollcoro_task), asio::use_awaitable);

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
            int value = co_await from_asio(executor, []() -> asio::awaitable<int> {
                auto exec = co_await asio::this_coro::executor;
                asio::steady_timer timer(exec, std::chrono::milliseconds(100));
                co_await timer.async_wait(asio::use_awaitable);
                co_return 21;
            });

            log("pollcoro_task: got value ", value, " from inner ASIO");
            co_return value;
        };

        // Convert pollcoro -> ASIO awaitable
        int result = co_await to_asio(executor, pollcoro_task(), asio::use_awaitable);

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

            int value = co_await to_asio(executor, compute(), asio::use_awaitable);
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
        int result = co_await to_asio(executor, pollcoro_task(), asio::use_awaitable);

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
                co_return co_await to_asio(
                    co_await asio::this_coro::executor, task_a(1), asio::use_awaitable
                );
            },
            asio::use_future
        );

        auto future_b = asio::co_spawn(
            executor,
            [&]() -> asio::awaitable<int> {
                co_return co_await to_asio(
                    co_await asio::this_coro::executor, task_a(2), asio::use_awaitable
                );
            },
            asio::use_future
        );

        auto future_c = asio::co_spawn(
            executor,
            [&]() -> asio::awaitable<int> {
                co_return co_await to_asio(
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
            auto value = co_await from_asio(executor, [i]() -> asio::awaitable<int> {
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
            auto asio_result = co_await from_asio(executor, []() -> asio::awaitable<int> {
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

        int result = co_await to_asio(executor, pollcoro_outer(), asio::use_awaitable);

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
