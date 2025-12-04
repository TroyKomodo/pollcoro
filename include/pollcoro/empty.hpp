#pragma once

#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class empty_stream_awaitable : public awaitable_never_blocks {
        using state_type = stream_awaitable_state<T>;

      public:
        state_type poll_next(const waker& w) {
            return state_type::done();
        }
    };

    template<typename T>
    constexpr auto empty() {
        return empty_stream_awaitable<T>();
    }
}