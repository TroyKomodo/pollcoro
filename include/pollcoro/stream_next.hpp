#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "stream.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class stream_next_awaitable {
        stream<T>& stream_;

        using result_type = std::optional<T>;
        using state_type = awaitable_state<result_type>;

      public:
        stream_next_awaitable(stream<T>& stream) : stream_(stream) {}

        state_type poll(const waker& w) {
            auto state = stream_.poll_next(w);
            if (state.is_done()) {
                return state_type::ready(std::nullopt);
            }
            if (state.is_ready()) {
                return state_type::ready(std::make_optional(state.take_result()));
            }
            return state_type::pending();
        }
    };

    template<typename T>
    constexpr auto stream_next(stream<T> & s) {
        return stream_next_awaitable<T>(s);
    }

}  // namespace pollcoro
