#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <atomic>
#include <coroutine>
#include <type_traits>
#include <variant>
#endif

#include "awaitable.hpp"
#include "co_awaitable.hpp"
#include "export.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    namespace detail {
    template<typename T>
    struct resumable_promise_storage {
        std::variant<std::monostate, T, std::exception_ptr> result;

        void return_value(T value) {
            result.template emplace<T>(std::move(value));
        }

        void unhandled_exception() {
            result.template emplace<std::exception_ptr>(std::current_exception());
        }

        T take_result() {
            if (std::holds_alternative<std::exception_ptr>(result)) {
                std::rethrow_exception(std::get<std::exception_ptr>(result));
            }
            return std::move(std::get<T>(result));
        }

        bool has_value() const noexcept {
            return !std::holds_alternative<std::monostate>(result);
        }
    };

    template<>
    struct resumable_promise_storage<void> {
        std::variant<std::monostate, std::exception_ptr> result;
        bool completed = false;

        void return_void() {
            completed = true;
        }

        void unhandled_exception() {
            result.template emplace<std::exception_ptr>(std::current_exception());
        }

        void take_result() {
            if (std::holds_alternative<std::exception_ptr>(result)) {
                std::rethrow_exception(std::get<std::exception_ptr>(result));
            }
        }

        bool has_value() const noexcept {
            return completed || std::holds_alternative<std::exception_ptr>(result);
        }
    };
    }  // namespace detail

    template<POLLCORO_CONCEPT(awaitable) Awaitable, POLLCORO_CONCEPT(co_scheduler) Scheduler>
    auto make_resumable(Awaitable && awaitable, Scheduler & scheduler) {
        using result_type = awaitable_result_t<std::decay_t<Awaitable>>;

        struct task_t {
            struct promise_type : detail::resumable_promise_storage<result_type> {
                std::coroutine_handle<> continuation;
                std::atomic<bool> resumed{false};

                task_t get_return_object() {
                    return task_t{std::coroutine_handle<promise_type>::from_promise(*this)};
                }

                std::suspend_always initial_suspend() noexcept {
                    return {};
                }

                auto final_suspend() noexcept {
                    struct final_awaiter {
                        bool await_ready() const noexcept {
                            return false;
                        }

                        std::coroutine_handle<>
                        await_suspend(std::coroutine_handle<promise_type> handle) const noexcept {
                            auto cont = handle.promise().continuation;
                            return cont ? cont : std::noop_coroutine();
                        }

                        void await_resume() const noexcept {}
                    };

                    return final_awaiter{};
                }
            };

            std::coroutine_handle<promise_type> handle_;

            explicit task_t(std::coroutine_handle<promise_type> handle) noexcept
                : handle_(handle) {}

            ~task_t() {
                if (handle_) {
                    handle_.destroy();
                }
            }

            task_t(task_t&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

            task_t& operator=(task_t&& other) noexcept {
                if (this != &other) {
                    if (handle_) {
                        handle_.destroy();
                    }
                    handle_ = std::exchange(other.handle_, nullptr);
                }
                return *this;
            }

            task_t(const task_t&) = delete;
            task_t& operator=(const task_t&) = delete;

            bool await_ready() const noexcept {
                return handle_.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) noexcept {
                handle_.promise().continuation = handle;
                return handle_;
            }

            result_type await_resume() {
                return handle_.promise().take_result();
            }

            void start() {
                handle_.resume();
            }

            bool done() const noexcept {
                return handle_.done();
            }
        };

        auto coro = [](std::decay_t<Awaitable> inner_awaitable, Scheduler& sched) -> task_t {
            struct poll_awaiter {
                std::decay_t<Awaitable>& awaitable;
                mutable awaitable_state<result_type> state;

                bool await_ready() const noexcept {
                    return false;
                }

                bool await_suspend(std::coroutine_handle<typename task_t::promise_type> handle) {
                    handle.promise().resumed.store(false, std::memory_order_relaxed);

                    state = awaitable.poll(waker(handle.address(), [](void* data) noexcept {
                        auto h = std::coroutine_handle<typename task_t::promise_type>::from_address(
                            data
                        );
                        bool expected = false;
                        if (h.promise().resumed.compare_exchange_strong(
                                expected, true, std::memory_order_acq_rel
                            )) {
                            h.resume();
                        }
                    }));

                    // If already ready, resume immediately
                    if (state.is_ready()) {
                        return false;
                    }
                    return true;
                }

                awaitable_state<result_type> await_resume() {
                    return std::move(state);
                }
            };

            while (true) {
                auto state = co_await poll_awaiter{inner_awaitable, {}};
                if (state.is_ready()) {
                    if constexpr (std::is_void_v<result_type>) {
                        co_return;
                    } else {
                        co_return state.take_result();
                    }
                }
                // Yield to scheduler before next poll
                co_await sched.schedule();
            }
        }(std::forward<Awaitable>(awaitable), scheduler);

        return coro;
    }
}  // namespace pollcoro