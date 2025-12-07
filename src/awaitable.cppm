module;

#include <concepts>
#include <optional>
#include <tuple>
#include <type_traits>

export module pollcoro:awaitable;

import :waker;

export namespace pollcoro {
template<typename T = void>
class awaitable_state {
    constexpr explicit awaitable_state(T result) : result_(std::move(result)) {}

  public:
    using result_type = T;
    awaitable_state() = default;

    static constexpr awaitable_state ready(T result) {
        return awaitable_state(std::move(result));
    }

    static constexpr awaitable_state pending() {
        return awaitable_state();
    }

    bool is_ready() const {
        return result_.has_value();
    }

    T take_result() {
        return std::move(*result_);
    }

    template<typename Func>
    auto map(Func&& func) && {
        using U = std::invoke_result_t<Func, T>;
        if (!is_ready()) {
            return awaitable_state<U>::pending();
        }
        return awaitable_state<U>::ready(func(take_result()));
    }

  private:
    std::optional<T> result_ = std::nullopt;
};

template<>
class awaitable_state<void> {
    constexpr awaitable_state(bool ready) : ready_(ready) {}

  public:
    using result_type = void;

    awaitable_state() = default;

    static constexpr awaitable_state ready() {
        return awaitable_state(true);
    }

    static constexpr awaitable_state pending() {
        return awaitable_state(false);
    }

    bool is_ready() const {
        return ready_;
    }

    void take_result() {}

    template<typename Func>
    auto map(Func&& func) && {
        using U = std::invoke_result_t<Func>;
        if (!is_ready()) {
            return awaitable_state<U>::pending();
        }
        return awaitable_state<U>::ready(func());
    }

  private:
    bool ready_{false};
};

namespace detail {
template<typename T>
inline constexpr bool is_awaitable_state_v = false;

template<typename T>
inline constexpr bool is_awaitable_state_v<awaitable_state<T>> = true;

template<typename T>
struct awaitable_state_traits;

template<typename T>
struct awaitable_state_traits<awaitable_state<T>> {
    using result_type = T;
};

}  // namespace detail

template<typename T>
concept awaitable = requires(T t, const waker& w) {
    { t.poll(w) } -> std::same_as<awaitable_state<typename decltype(t.poll(w))::result_type>>;
} && detail::is_awaitable_state_v<decltype(std::declval<T>().poll(std::declval<const waker&>()))>;

template<awaitable T>
using awaitable_result_t = typename detail::awaitable_state_traits<
    decltype(std::declval<T>().poll(std::declval<const waker&>()))>::result_type;

}  // namespace pollcoro
