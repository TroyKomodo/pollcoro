#pragma once

#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class range_stream_awaitable : public awaitable_never_blocks {
        using value_type = T;
        using state_type = stream_awaitable_state<value_type>;
        T current_;
        T end_;

      public:
        range_stream_awaitable(T begin, T end) : current_(begin), end_(end) {}

        state_type poll_next(const waker& w) {
            if (current_ >= end_) {
                return state_type::done();
            }
            return state_type::ready(current_++);
        }
    };

    template<typename T>
    constexpr auto range(T begin, T end) {
        return range_stream_awaitable<T>(begin, end);
    }

    template<typename T>
    constexpr auto range(T end) {
        return range_stream_awaitable<T>(T(0), end);
    }
}
