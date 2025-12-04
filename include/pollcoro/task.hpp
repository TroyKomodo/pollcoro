#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <utility>
#endif

#include "awaitable.hpp"
#include "detail/promise.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    namespace detail {}  // namespace detail

    template<typename T = void>
    class task : public awaitable_always_blocks {
      public:
        using promise_type = detail::promise_type<task, T, detail::task_storage<T>>;

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

        awaitable_state<T> poll(const waker& w) {
            auto& promise = handle_.promise();
            bool resumed = false;
            if (!is_ready()) {
                try {
                    resumed = promise.poll_ready(w);
                } catch (...) {
                    promise.exception = std::current_exception();
                    resumed = true;
                }
                if (resumed) {
                    handle_.resume();
                }
            }

            if (is_ready()) {
                if constexpr (std::is_void_v<T>) {
                    return awaitable_state<T>::ready();
                } else {
                    return awaitable_state<T>::ready(promise.take_result());
                }
            }

            if (resumed) {
                w.wake();
            }

            return awaitable_state<T>::pending();
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
    };

}  // namespace pollcoro
