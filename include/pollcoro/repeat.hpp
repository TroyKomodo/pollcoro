#pragma once

#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class repeat_stream_awaitable : public awaitable_never_blocks {
        T value_;
        using state_type = stream_awaitable_state<T>;

      public:
        repeat_stream_awaitable(T value) : value_(value) {}

        state_type poll_next(const waker& w) {
            return state_type::ready(value_);
        }
    };

    template<typename T>
    constexpr auto repeat(T value) {
        return repeat_stream_awaitable<T>(value);
    }
}