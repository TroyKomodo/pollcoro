#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <iterator>
#endif

#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    namespace detail {
    enum class iter_mode {
        copy,
        move
    };
    }  // namespace detail

    template<typename Iterator, detail::iter_mode Mode>
    class iter_stream_awaitable : public awaitable_never_blocks {
        using value_type = typename std::iterator_traits<Iterator>::value_type;
        using state_type = stream_awaitable_state<value_type>;
        Iterator current_;
        Iterator end_;

      public:
        constexpr iter_stream_awaitable(Iterator begin, Iterator end)
            : current_(begin), end_(end) {}

        constexpr iter_stream_awaitable(iter_stream_awaitable&&) = default;
        constexpr iter_stream_awaitable& operator=(iter_stream_awaitable&&) = default;
        constexpr iter_stream_awaitable(const iter_stream_awaitable&) = delete;
        constexpr iter_stream_awaitable& operator=(const iter_stream_awaitable&) = delete;

        constexpr state_type poll_next(const waker&) {
            if (current_ == end_) {
                return state_type::done();
            }
            if constexpr (Mode == detail::iter_mode::move) {
                return state_type::ready(std::move(*current_++));
            } else {
                return state_type::ready(*current_++);
            }
        }
    };

    // From iterator pair (copy)
    template<typename Iterator>
    constexpr auto iter(Iterator begin, Iterator end) {
        return iter_stream_awaitable<Iterator, detail::iter_mode::copy>(begin, end);
    }

    // From range/container (copy)
    template<typename Range>
    constexpr auto iter(const Range& range) {
        return iter(std::begin(range), std::end(range));
    }

    // From iterator pair (move)
    template<typename Iterator>
    constexpr auto iter_move(Iterator begin, Iterator end) {
        return iter_stream_awaitable<Iterator, detail::iter_mode::move>(begin, end);
    }

    // From range/container (move)
    template<typename Range>
    constexpr auto iter_move(Range & range) {
        return iter_move(std::begin(range), std::end(range));
    }

}  // namespace pollcoro
