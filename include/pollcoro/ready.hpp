#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class ready_awaitable : public awaitable_never_blocks {
        T value_;

      public:
        ready_awaitable(T value) : value_(std::move(value)) {}

        awaitable_state<T> poll(const waker& w) {
            return awaitable_state<T>::ready(std::move(value_));
        }
    };

    template<>
    class ready_awaitable<void> : public awaitable_never_blocks {
      public:
        awaitable_state<> poll(const waker& w) {
            return awaitable_state<>::ready();
        }
    };

    template<typename T>
    constexpr auto ready(T value) {
        return ready_awaitable<T>(std::move(value));
    }

    constexpr auto ready() {
        return ready_awaitable<void>();
    }
}  // namespace pollcoro
