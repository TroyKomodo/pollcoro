#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <iterator>
#include <type_traits>
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

    struct empty_storage {};

    template<typename Iterator, iter_mode Mode, typename Storage = void>
    class iter_stream_awaitable : public awaitable_never_blocks {
        using value_type = typename std::iterator_traits<Iterator>::value_type;
        using state_type = stream_awaitable_state<value_type>;
        static constexpr bool owns_storage = !std::is_void<Storage>::value;
        using storage_type = typename std::conditional<owns_storage, Storage, empty_storage>::type;

        [[no_unique_address]] storage_type storage_;
        Iterator current_;
        Iterator end_;

      public:
        // Non-owning constructor
        template<
            typename S = Storage,
            typename = typename std::enable_if<std::is_void<S>::value>::type>
        constexpr iter_stream_awaitable(Iterator begin, Iterator end)
            : storage_{}, current_(begin), end_(end) {}

        // Owning constructor
        template<
            typename S = Storage,
            typename = typename std::enable_if<!std::is_void<S>::value>::type>
        constexpr iter_stream_awaitable(S&& container)
            : storage_(std::move(container)),
              current_(std::begin(storage_)),
              end_(std::end(storage_)) {}

        // Non-owning move constructor
        template<
            typename S = Storage,
            typename = typename std::enable_if<std::is_void<S>::value>::type>
        constexpr iter_stream_awaitable(iter_stream_awaitable&& other, S* = nullptr)
            : storage_{}, current_(other.current_), end_(other.end_) {}

        // Owning move constructor
        template<
            typename S = Storage,
            typename = typename std::enable_if<!std::is_void<S>::value>::type>
        constexpr iter_stream_awaitable(iter_stream_awaitable&& other, int = 0)
            : storage_(std::move(other.storage_)),
              current_(
                  std::next(
                      std::begin(storage_),
                      std::distance(std::begin(other.storage_), other.current_)
                  )
              ),
              end_(std::end(storage_)) {}

        constexpr iter_stream_awaitable& operator=(iter_stream_awaitable&&) = delete;
        constexpr iter_stream_awaitable(const iter_stream_awaitable&) = delete;
        constexpr iter_stream_awaitable& operator=(const iter_stream_awaitable&) = delete;

        constexpr state_type poll_next(const waker&) {
            if (current_ == end_) {
                return state_type::done();
            }
            if constexpr (Mode == iter_mode::move) {
                return state_type::ready(std::move(*current_++));
            } else {
                return state_type::ready(*current_++);
            }
        }
    };
    }  // namespace detail

    // From iterator pair (copy)
    template<typename Iterator>
    constexpr auto iter(Iterator begin, Iterator end) {
        return detail::iter_stream_awaitable<Iterator, detail::iter_mode::copy>(begin, end);
    }

    // From lvalue range (copy, non-owning)
    template<typename Range>
    constexpr auto iter(Range & range) {
        return iter(std::begin(range), std::end(range));
    }

    // From rvalue range (copy, owning)
    template<
        typename Range,
        typename = typename std::enable_if<!std::is_lvalue_reference<Range>::value>::type>
    constexpr auto iter(Range && range) {
        using Storage = typename std::remove_cv<typename std::remove_reference<Range>::type>::type;
        using Iter = decltype(std::begin(std::declval<Storage&>()));
        return detail::iter_stream_awaitable<Iter, detail::iter_mode::copy, Storage>(
            std::move(range)
        );
    }

    // From iterator pair (move)
    template<typename Iterator>
    constexpr auto iter_move(Iterator begin, Iterator end) {
        return detail::iter_stream_awaitable<Iterator, detail::iter_mode::move>(begin, end);
    }

    // From lvalue range (move, non-owning)
    template<typename Range>
    constexpr auto iter_move(Range & range) {
        return iter_move(std::begin(range), std::end(range));
    }

    // From rvalue range (move elements, owning container)
    template<
        typename Range,
        typename = typename std::enable_if<!std::is_lvalue_reference<Range>::value>::type>
    constexpr auto iter_move(Range && range) {
        using Storage = typename std::remove_cv<typename std::remove_reference<Range>::type>::type;
        using Iter = decltype(std::begin(std::declval<Storage&>()));
        return detail::iter_stream_awaitable<Iter, detail::iter_mode::move, Storage>(
            std::move(range)
        );
    }

}  // namespace pollcoro
