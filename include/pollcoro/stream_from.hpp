#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <iterator>
#endif

#include "export.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    // Lightweight iterator adapter - no coroutine frame needed
    template<typename Iterator>
    class into_stream_awaitable {
      public:
        using value_type = typename std::iterator_traits<Iterator>::value_type;

      private:
        Iterator current_;
        Iterator end_;

      public:
        constexpr into_stream_awaitable(Iterator begin, Iterator end) : current_(begin), end_(end) {}

        constexpr into_stream_awaitable(into_stream_awaitable&&) = default;
        constexpr into_stream_awaitable& operator=(into_stream_awaitable&&) = default;
        constexpr into_stream_awaitable(const into_stream_awaitable&) = delete;
        constexpr into_stream_awaitable& operator=(const into_stream_awaitable&) = delete;

        constexpr stream_awaitable_state<value_type> poll_next(const waker&) {
            if (current_ == end_) {
                return stream_awaitable_state<value_type>::done();
            }
            return stream_awaitable_state<value_type>::ready(*current_++);
        }
    };

    // yield_from helpers - create lightweight iterator sources

    // From iterator pair
    template<typename Iterator>
    constexpr auto stream_from(Iterator begin, Iterator end) {
        return into_stream_awaitable<Iterator>(begin, end);
    }

    // From range/container
    template<typename Range>
    constexpr auto stream_from(Range && range) {
        return stream_from(std::begin(range), std::end(range));
    }

}  // namespace pollcoro
