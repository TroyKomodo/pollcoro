#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class pending_awaitable {
      public:
        awaitable_state<T> poll(const waker& w) {
            return awaitable_state<T>::pending();
        }
    };

    template<typename T>
    constexpr auto pending() {
        return pending_awaitable<T>();
    }
}  // namespace pollcoro
