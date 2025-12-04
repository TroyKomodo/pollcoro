#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class pending_awaitable : public awaitable_always_blocks {
      public:
        awaitable_state<T> poll(const waker& w) {
            return awaitable_state<T>::pending();
        }
    };

    template<typename T>
    class pending_stream_awaitable : public awaitable_always_blocks {
        using state_type = stream_awaitable_state<T>;

      public:
        state_type poll_next(const waker& w) {
            return state_type::pending();
        }
    };

    template<typename T>
    constexpr auto pending() {
        return pending_awaitable<T>();
    }

    template<typename T>
    constexpr auto pending_stream() {
        return pending_stream_awaitable<T>();
    }
}  // namespace pollcoro
