#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <coroutine>
#include <memory>
#include <mutex>
#include <type_traits>
#include <variant>
#endif

#include "awaitable.hpp"
#include "co_awaitable.hpp"
#include "export.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    struct shared_state {
        using storage = std::conditional_t<std::is_void_v<T>, std::monostate, T>;
        using result_type = T;
        using state_type = pollcoro::awaitable_state<result_type>;

        std::mutex mutex;
        bool done = false;
        pollcoro::waker waker;
        std::variant<storage, std::exception_ptr> result;

        void set_result(storage result) {
            std::lock_guard lock(mutex);
            this->result = std::move(result);
            done = true;
            waker.wake();
        }

        void set_exception(std::exception_ptr exception) {
            std::lock_guard lock(mutex);
            this->result = std::move(exception);
            done = true;
            waker.wake();
        }

        state_type poll(const pollcoro::waker& w) {
            std::lock_guard lock(mutex);
            if (done) {
                if (std::holds_alternative<std::exception_ptr>(result)) {
                    std::rethrow_exception(std::get<std::exception_ptr>(result));
                }

                if constexpr (std::is_void_v<result_type>) {
                    return state_type::ready();
                } else {
                    return state_type::ready(std::move(std::get<storage>(result)));
                }
            }

            waker = w;
            return state_type::pending();
        }
    };

    template<typename task_t, typename shared_state_t>
    class make_pollable_adapter {
        task_t task_;
        std::shared_ptr<shared_state_t> state_;
        bool initialized_ = false;

        using result_type = shared_state_t::result_type;
        using state_type = pollcoro::awaitable_state<result_type>;

      public:
        make_pollable_adapter(task_t&& task, std::shared_ptr<shared_state_t> state)
            : task_(std::forward<task_t>(task)), state_(std::move(state)) {}

        state_type poll(const pollcoro::waker& w) {
            return state_->poll(w);
        }
    };

    template<typename Resumable>
    auto make_pollable(Resumable && resumable) {
        using result_type = co_await_result_t<Resumable>;
        using shared_state_t = shared_state<result_type>;
        auto state = std::make_shared<shared_state_t>();

        struct task_t {
            struct promise_type {
                task_t get_return_object() {
                    return task_t{std::coroutine_handle<promise_type>::from_promise(*this)};
                }

                std::suspend_always initial_suspend() {
                    return {};
                }

                std::suspend_always final_suspend() noexcept {
                    return {};
                }

                void return_void() {}

                void unhandled_exception() {
                    std::terminate();
                }
            };

            std::coroutine_handle<promise_type> handle_;

            task_t(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

            task_t(task_t&& other) noexcept : handle_(other.handle_) {
                other.handle_ = nullptr;
            }

            task_t& operator=(task_t&& other) noexcept {
                if (this != &other) {
                    handle_ = other.handle_;
                    other.handle_ = nullptr;
                }
                return *this;
            }

            task_t(const task_t&) = delete;
            task_t& operator=(const task_t&) = delete;

            ~task_t() {
                if (handle_) {
                    handle_.destroy();
                }
            }

            void resume() {
                handle_.resume();
            }
        };

        auto task = ([](Resumable resumable, std::shared_ptr<shared_state_t> state) -> task_t {
            try {
                if constexpr (std::is_void_v<result_type>) {
                    co_await resumable;
                    state->set_result(std::monostate{});
                } else {
                    state->set_result(co_await resumable);
                }
            } catch (...) {
                state->set_exception(std::current_exception());
            }
        })(std::forward<Resumable>(resumable), state);

        task.resume();

        return make_pollable_adapter<task_t, shared_state_t>(std::move(task), state);
    }
}