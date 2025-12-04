#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <utility>
#endif

#include "awaitable.hpp"
#include "export.hpp"
#include "gen_awaitable.hpp"
#include "task.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class generator {
      public:
        using promise_type = detail::promise_type<generator, T, detail::generator_storage<T>>;

        explicit generator(std::coroutine_handle<promise_type> h, bool destroy_on_drop = true)
            : handle_(h), destroy_on_drop_(destroy_on_drop) {}

        generator(generator&& other) noexcept
            : handle_(other.handle_), destroy_on_drop_(other.destroy_on_drop_) {
            other.handle_ = nullptr;
        }

        generator& operator=(generator&& other) noexcept {
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

        generator(const generator&) = delete;
        generator& operator=(const generator&) = delete;

        ~generator() {
            if (handle_ && destroy_on_drop_) {
                handle_.destroy();
            }
        }

        gen_awaitable_state<T> poll_next(const waker& w) {
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

            if (promise.has_value()) {
                return gen_awaitable_state<T>::ready(promise.take_result());
            }

            if (is_ready()) {
                return gen_awaitable_state<T>::done();
            }

            if (resumed) {
                w.wake();
            }

            return gen_awaitable_state<T>::pending();
        }

      private:
        std::coroutine_handle<promise_type> handle_;
        bool destroy_on_drop_{true};

        bool is_ready() const {
            return handle_.done();
        }
    };

}  // namespace pollcoro
