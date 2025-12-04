#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <iterator>
#endif

#include "export.hpp"
#include "gen_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    // Lightweight iterator adapter - no coroutine frame needed
    template<typename Iterator>
    class into_gen_awaitable {
      public:
        using value_type = typename std::iterator_traits<Iterator>::value_type;

      private:
        Iterator current_;
        Iterator end_;

      public:
        constexpr into_gen_awaitable(Iterator begin, Iterator end) : current_(begin), end_(end) {}

        constexpr into_gen_awaitable(into_gen_awaitable&&) = default;
        constexpr into_gen_awaitable& operator=(into_gen_awaitable&&) = default;
        constexpr into_gen_awaitable(const into_gen_awaitable&) = delete;
        constexpr into_gen_awaitable& operator=(const into_gen_awaitable&) = delete;

        constexpr gen_awaitable_state<value_type> poll_next(const waker&) {
            if (current_ == end_) {
                return gen_awaitable_state<value_type>::done();
            }
            return gen_awaitable_state<value_type>::ready(*current_++);
        }
    };

    // yield_from helpers - create lightweight iterator sources

    // From iterator pair
    template<typename Iterator>
    constexpr auto gen(Iterator begin, Iterator end) {
        return into_gen_awaitable<Iterator>(begin, end);
    }

    // From range/container
    template<typename Range>
    constexpr auto gen(Range && range) {
        return gen(std::begin(range), std::end(range));
    }

}  // namespace pollcoro
