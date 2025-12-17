module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#endif

export module pollcoro:stream_awaitable;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :waker;

export namespace pollcoro {
template<typename T>
class stream_awaitable_state {
    constexpr explicit stream_awaitable_state(T result) : result_(std::move(result)) {}

    explicit stream_awaitable_state(bool done) : done_(done) {}

  public:
    using result_type = T;
    stream_awaitable_state() = default;

    static constexpr stream_awaitable_state ready(T result) {
        return stream_awaitable_state(std::move(result));
    }

    static constexpr stream_awaitable_state pending() {
        return stream_awaitable_state(false);
    }

    static constexpr stream_awaitable_state done() {
        return stream_awaitable_state(true);
    }

    bool is_ready() const {
        return result_.has_value();
    }

    bool is_done() const {
        return done_;
    }

    T take_result() {
        return *std::exchange(result_, std::nullopt);
    }

    template<typename Func>
    auto map(Func&& func) && {
        using U = std::invoke_result_t<Func, T>;
        if (is_done()) {
            return stream_awaitable_state<U>::done();
        }
        if (!is_ready()) {
            return stream_awaitable_state<U>::pending();
        }

        return stream_awaitable_state<U>::ready(func(take_result()));
    }

  private:
    std::optional<T> result_ = std::nullopt;
    bool done_{false};
};

namespace detail {
template<typename T>
inline constexpr bool is_stream_awaitable_state_v = false;

template<typename T>
inline constexpr bool is_stream_awaitable_state_v<stream_awaitable_state<T>> = true;

template<typename T>
struct stream_awaitable_state_traits;

template<typename T>
struct stream_awaitable_state_traits<stream_awaitable_state<T>> {
    using result_type = T;
};

template<typename T>
concept is_stream_awaitable_state = is_stream_awaitable_state_v<T>;

}  // namespace detail

template<typename T>
concept stream_awaitable = std::move_constructible<T> && requires(T t, const waker& w) {
    { t.poll_next(w) } -> detail::is_stream_awaitable_state;
};

template<stream_awaitable T>
using stream_awaitable_result_t = typename detail::stream_awaitable_state_traits<
    decltype(std::declval<T>().poll_next(std::declval<const waker&>()))>::result_type;

}  // namespace pollcoro
