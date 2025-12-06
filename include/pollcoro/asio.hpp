#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <atomic>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <variant>
#endif

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    // =============================================================================
    // Detail namespace for ASIO interop
    // =============================================================================

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
        waker stored_waker;
        std::variant<std::monostate, storage, std::exception_ptr> result;

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

        awaitable_state<T> poll(const waker& w) {
            std::lock_guard lock(mutex);
            if (done) {
                if (std::holds_alternative<std::exception_ptr>(result)) {
                    std::rethrow_exception(std::get<std::exception_ptr>(result));
                }
                if constexpr (std::is_void_v<T>) {
                    return awaitable_state<T>::ready();
                } else {
                    return awaitable_state<T>::ready(std::move(std::get<storage>(result)));
                }
            }
            stored_waker = w;
            return awaitable_state<T>::pending();
        }
    };

    }  // namespace detail

    // =============================================================================
    // from_asio: Convert an ASIO awaitable to a pollcoro awaitable
    // =============================================================================

    /// Adapter that wraps an ASIO awaitable factory for use in pollcoro coroutines.
    /// The ASIO operation is spawned on the provided executor and runs independently.
    /// Poll returns ready when the ASIO operation completes.
    template<typename T>
    class from_asio_awaitable : public awaitable_always_blocks {
        using state_type = detail::asio_shared_state<T>;
        std::shared_ptr<state_type> state_;
        asio::any_io_executor executor_;
        std::function<asio::awaitable<T>()> factory_;

      public:
        from_asio_awaitable(
            asio::any_io_executor executor, std::function<asio::awaitable<T>()> factory
        )
            : state_(std::make_shared<state_type>()),
              executor_(std::move(executor)),
              factory_(std::move(factory)) {}

        from_asio_awaitable(const from_asio_awaitable&) = delete;
        from_asio_awaitable& operator=(const from_asio_awaitable&) = delete;
        from_asio_awaitable(from_asio_awaitable&&) = default;
        from_asio_awaitable& operator=(from_asio_awaitable&&) = default;

        awaitable_state<T> poll(const waker& w) {
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
                        co_await factory();
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
                        co_return co_await factory();
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

    /// Convert an ASIO awaitable factory to a pollcoro awaitable.
    /// The factory is a callable that returns an asio::awaitable<T>.
    ///
    /// Example:
    ///   auto executor = ctx.get_executor();
    ///   auto pollable = pollcoro::from_asio<int>(executor, [&]() -> asio::awaitable<int> {
    ///       asio::steady_timer timer(co_await asio::this_coro::executor,
    ///                                std::chrono::milliseconds(100));
    ///       co_await timer.async_wait(asio::use_awaitable);
    ///       co_return 42;
    ///   });
    ///   int result = co_await pollable;  // in a pollcoro task
    template<typename T>
    auto from_asio(asio::any_io_executor executor, std::function<asio::awaitable<T>()> factory) {
        return from_asio_awaitable<T>(std::move(executor), std::move(factory));
    }

    /// Convenience overload that takes an io_context reference
    template<typename T>
    auto from_asio(asio::io_context & ctx, std::function<asio::awaitable<T>()> factory) {
        return from_asio_awaitable<T>(ctx.get_executor(), std::move(factory));
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
    ///       co_return co_await pollcoro::to_asio(
    ///           executor, my_pollcoro_task(), asio::use_awaitable
    ///       );
    ///   }
    template<awaitable Awaitable, typename CompletionToken>
    auto to_asio(asio::any_io_executor executor, Awaitable && aw, CompletionToken && token) {
        using result_type = awaitable_result_t<std::decay_t<Awaitable>>;
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
                        : awaitable(std::move(aw)),
                          executor(std::move(exec)),
                          handler_(std::move(h)) {}

                    void poll() {
                        while (wake_pending.exchange(false, std::memory_order_release)) {
                            polling = true;
                            try {
                                auto state = awaitable.poll(waker(this));
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

                    void complete(awaitable_state<result_type> state) {
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

    // =============================================================================
    // asio_scheduler: Scheduler adapter for make_resumable with ASIO
    // =============================================================================

    /// Scheduler adapter for use with make_resumable.
    /// Allows pollcoro awaitables to be used with resume-based coroutines
    /// that use ASIO for scheduling.
    ///
    /// Example:
    ///   // In a non-ASIO coroutine that uses ASIO for I/O
    ///   asio_scheduler sched{ctx.get_executor()};
    ///   co_await pollcoro::make_resumable(my_pollcoro_task(), sched);
    struct asio_scheduler {
        asio::any_io_executor executor;

        auto schedule() {
            struct schedule_awaitable {
                asio::any_io_executor exec;

                bool await_ready() const noexcept {
                    return false;
                }

                void await_suspend(std::coroutine_handle<> h) const {
                    asio::post(exec, [h]() {
                        h.resume();
                    });
                }

                void await_resume() const noexcept {}
            };

            return schedule_awaitable{executor};
        }
    };

}  // namespace pollcoro
