#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <utility>
#endif

#include "../awaitable.hpp"
#include "../gen_awaitable.hpp"
#include "../waker.hpp"

namespace pollcoro::detail {
struct promise_base {
    std::exception_ptr exception{nullptr};
    std::function<bool(const waker&)> current_awaitable_poll{nullptr};
};

template<typename T>
class task_storage : public promise_base {
    std::optional<T> result;

  public:
    void return_value(T value) {
        result = std::move(value);
    }

    T take_result() {
        auto value = std::move(*result);
        result = std::nullopt;
        return value;
    }
};

template<>
class task_storage<void> : public promise_base {
  public:
    void return_void() {}
};

template<typename T>
class generator_storage : public promise_base {
    std::optional<T> result;

  public:
    void return_void() {}

    template<POLLCORO_CONCEPT(gen_awaitable) GenAwaitable>
    auto yield_value(GenAwaitable&& gen_awaitable) {
        POLLCORO_STATIC_ASSERT_GEN(GenAwaitable);

        using result_type = gen_awaitable_result_t<GenAwaitable>;

        struct transformed_promise {
            generator_storage& promise;
            std::decay_t<GenAwaitable> gen_awaitable;

            transformed_promise(generator_storage& promise, GenAwaitable&& gen_awaitable)
                : promise(promise), gen_awaitable(std::forward<GenAwaitable>(gen_awaitable)) {}

            constexpr bool await_ready() {
                return false;
            }

            void await_suspend(std::coroutine_handle<>) {
                promise.exception = nullptr;
                promise.current_awaitable_poll = [this](const waker& w) {
                    auto state = gen_awaitable.poll_next(w);
                    if (state.is_done()) {
                        return true;
                    }
                    if (state.is_ready()) {
                        promise.yield_value(state.take_result());
                    }

                    return false;
                };
            }

            void await_resume() {
                auto exception = promise.exception;
                promise.current_awaitable_poll = nullptr;
                promise.exception = nullptr;
                if (exception) {
                    std::rethrow_exception(exception);
                }
            }
        };

        return transformed_promise(*this, std::forward<GenAwaitable>(gen_awaitable));
    }

    std::suspend_always yield_value(T value) {
        result = std::move(value);
        return {};
    }

    T take_result() {
        auto value = std::move(*result);
        result = std::nullopt;
        return value;
    }

    bool has_value() const {
        return result.has_value();
    }
};

template<typename promise_type, POLLCORO_CONCEPT(awaitable) Awaitable>
auto transform_awaitable(promise_type& promise, Awaitable&& awaitable) {
    POLLCORO_STATIC_ASSERT(Awaitable);

    using result_type = awaitable_result_t<Awaitable>;

    struct transformed_promise : task_storage<result_type> {
        promise_type& promise;
        std::decay_t<Awaitable> awaitable;

        transformed_promise(promise_type& promise, Awaitable&& awaitable)
            : promise(promise), awaitable(std::forward<Awaitable>(awaitable)) {}

        constexpr bool await_ready() {
            return false;
        }

        void await_suspend(std::coroutine_handle<>) {
            promise.exception = nullptr;
            promise.current_awaitable_poll = [this](const waker& w) {
                auto state = awaitable.poll(w);
                if (state.is_ready()) {
                    if constexpr (!std::is_void_v<result_type>) {
                        this->return_value(state.take_result());
                    }
                    return true;
                } else {
                    return false;
                }
            };
        }

        result_type await_resume() {
            auto exception = promise.exception;
            promise.current_awaitable_poll = nullptr;
            promise.exception = nullptr;
            if (exception) {
                std::rethrow_exception(exception);
            }
            if constexpr (!std::is_void_v<result_type>) {
                return this->take_result();
            }
        }
    };

    return transformed_promise(promise, std::forward<Awaitable>(awaitable));
}

template<typename task, typename result, typename storage>
struct promise_type : public storage {
    bool poll_ready(const waker& w) {
        if (this->current_awaitable_poll) {
            return this->current_awaitable_poll(w);
        }
        return true;
    }

    task get_return_object() {
        return task(std::coroutine_handle<promise_type>::from_promise(*this));
    }

    std::suspend_always initial_suspend() {
        return {};
    }

    std::suspend_always final_suspend() noexcept {
        return {};
    }

    void unhandled_exception() {
        this->exception = std::current_exception();
    }

    template<POLLCORO_CONCEPT(awaitable) Awaitable>
    auto await_transform(Awaitable&& awaitable) {
        POLLCORO_STATIC_ASSERT(Awaitable);
        return transform_awaitable<promise_type, Awaitable>(
            *this, std::forward<Awaitable>(awaitable)
        );
    }
};

}  // namespace pollcoro::detail