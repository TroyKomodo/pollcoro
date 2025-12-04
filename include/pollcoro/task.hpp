#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <utility>
#endif

#include "concept.hpp"
#include "export.hpp"
#include "pollable_state.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    namespace detail {

    template<typename T>
    class pollable_task_promise_mixin {
        std::optional<T> result;

      public:
        void return_value(T value) {
            result = std::move(value);
        }

        T take_result() {
            return std::move(*result);
        }
    };

    template<>
    class pollable_task_promise_mixin<void> {
      public:
        void return_void() {}

        void take_result() {}
    };

    template<POLLCORO_CONCEPT(awaitable) Awaitable, typename Promise>
    class waker_propagating_awaitable
        : public pollable_task_promise_mixin<awaitable_result_t<Awaitable>> {
        std::decay_t<Awaitable> awaitable;
        Promise& promise;

      public:
        waker_propagating_awaitable(Awaitable&& awaitable, Promise& promise)
            : awaitable(std::forward<Awaitable>(awaitable)), promise(promise) {}

        bool await_ready() {
            return false;
        }

        void await_suspend(std::coroutine_handle<>) {
            // Set up readiness check and waker propagation
            promise.current_awaitable_on_poll_ready = [this](const waker& w) {
                auto result = awaitable.on_poll(w);
                if (result.is_ready()) {
                    if constexpr (std::is_void_v<awaitable_result_t<Awaitable>>) {
                        result.take_result();
                        this->return_void();
                    } else {
                        this->return_value(result.take_result());
                    }
                    return true;
                } else {
                    return false;
                }
            };
        }

        awaitable_result_t<Awaitable> await_resume() {
            this->promise.current_awaitable_on_poll_ready = nullptr;
            if constexpr (std::is_void_v<awaitable_result_t<Awaitable>>) {
                this->take_result();
                return;
            } else {
                return this->take_result();
            }
        }
    };

    }  // namespace detail

    template<typename T = void>
    class task {
      public:
        struct promise_type : public detail::pollable_task_promise_mixin<T> {
            std::exception_ptr exception;

            std::function<bool(const waker&)> current_awaitable_on_poll_ready;

            bool poll_ready(const waker& w) {
                if (current_awaitable_on_poll_ready) {
                    return current_awaitable_on_poll_ready(w);
                }
                return true;
            }

            task get_return_object() {
                return task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            std::suspend_always initial_suspend() {
                return {};
            }

            std::suspend_always final_suspend() noexcept {
                return {};
            }

            void unhandled_exception() {
                exception = std::current_exception();
            }

            template<POLLCORO_CONCEPT(awaitable) Awaitable>
            auto await_transform(Awaitable&& awaitable) {
                POLLCORO_STATIC_ASSERT(Awaitable);

                return detail::waker_propagating_awaitable<Awaitable, promise_type>(
                    std::forward<Awaitable>(awaitable), *this
                );
            }
        };

        explicit task(std::coroutine_handle<promise_type> h, bool destroy_on_drop = true)
            : handle_(h), destroy_on_drop_(destroy_on_drop) {}

        task(task&& other) noexcept
            : handle_(other.handle_), destroy_on_drop_(other.destroy_on_drop_) {
            other.handle_ = nullptr;
        }

        task& operator=(task&& other) noexcept {
            if (this != &other) {
                if (handle_ && destroy_on_drop_) {
                    handle_.destroy();
                }
                handle_ = other.handle_;
                destroy_on_drop_ = other.destroy_on_drop_;
                other.handle_ = nullptr;
            }
            return *this;
        }

        task(const task&) = delete;
        task& operator=(const task&) = delete;

        ~task() {
            if (handle_ && destroy_on_drop_) {
                handle_.destroy();
            }
        }

        pollable_state<T> on_poll(const waker& w) {
            auto& promise = handle_.promise();
            bool resumed = false;
            if (!is_ready()) {
                if (promise.poll_ready(w)) {
                    resumed = true;
                    handle_.resume();
                }
            }

            if (is_ready()) {
                if constexpr (std::is_void_v<T>) {
                    take_result();
                    return pollable_state<T>::ready();
                } else {
                    return pollable_state<T>::ready(take_result());
                }
            } else if (resumed) {
                w.wake();
            }

            return pollable_state<T>::pending();
        }

        // The task will be left in an empty state after this call.
        // Useful if you want to work with the underlying coroutine handle.
        std::coroutine_handle<promise_type> release() && noexcept {
            return std::exchange(handle_, nullptr);
        }

      private:
        std::coroutine_handle<promise_type> handle_;
        bool destroy_on_drop_{true};

        bool is_ready() const {
            return handle_.done();
        }

        template<typename U = T>
        std::enable_if_t<!std::is_void_v<U>, U> take_result() {
            if (handle_.promise().exception) {
                std::rethrow_exception(handle_.promise().exception);
            }
            return handle_.promise().take_result();
        }

        template<typename U = T>
        std::enable_if_t<std::is_void_v<U>, void> take_result() {
            if (handle_.promise().exception) {
                std::rethrow_exception(handle_.promise().exception);
            }
            handle_.promise().take_result();
        }
    };

}  // namespace pollcoro
