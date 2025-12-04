#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "generator.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T>
    class gen_next_awaitable {
        generator<T>& gen_;

        using result_type = std::optional<T>;
        using state_type = awaitable_state<result_type>;

      public:
        gen_next_awaitable(generator<T>& gen) : gen_(gen) {}

        state_type poll(const waker& w) {
            auto state = gen_.poll_next(w);
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
    constexpr auto gen_next(generator<T> & gen) {
        return gen_next_awaitable<T>(gen);
    }

}  // namespace pollcoro