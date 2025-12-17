module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <cstddef>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#endif

export module pollcoro:wait_first;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :awaitable;
import :is_blocking;
import :waker;

export namespace pollcoro {
template<awaitable... Awaitables>
class wait_first_awaitable : public awaitable_maybe_blocks<Awaitables...> {
    static_assert(sizeof...(Awaitables) > 0, "wait_first requires at least one awaitable");

    // All awaitables must have the same result type for wait_first
    using first_result_t = awaitable_result_t<std::tuple_element_t<0, std::tuple<Awaitables...>>>;

  public:
    using result_type = std::tuple<first_result_t, std::size_t>;

    explicit wait_first_awaitable(Awaitables&&... awaitables)
        : awaitables_(std::move(awaitables)...) {}

    awaitable_state<result_type> poll(const waker& w) {
        return get_first_ready_result(std::index_sequence_for<Awaitables...>{}, w);
    }

  private:
    template<std::size_t I>
    auto try_get_result(const waker& w) {
        return std::get<I>(awaitables_).poll(w).map([](auto result) {
            return std::make_tuple(std::move(result), I);
        });
    }

    template<std::size_t... Is>
    auto get_first_ready_result(std::index_sequence<Is...>, const waker& w) {
        awaitable_state<result_type> result = awaitable_state<result_type>::pending();
        ((result = try_get_result<Is>(w), result.is_ready()) || ...);
        return std::move(result);
    }

    std::tuple<Awaitables...> awaitables_;
    bool completed_{false};
};

template<typename VecType>
requires requires(VecType& v) { *std::begin(v); }
class wait_first_iter_awaitable
    : public awaitable_maybe_blocks<decltype(*std::begin(std::declval<VecType&>()))> {
    using Awaitable = decltype(*std::begin(std::declval<VecType&>()));
    using result_t = awaitable_result_t<Awaitable>;

  public:
    using result_type = std::tuple<result_t, std::size_t>;

    explicit wait_first_iter_awaitable(VecType& awaitables) : awaitables_(awaitables) {}

    awaitable_state<result_type> poll(const waker& w) {
        std::size_t i = 0;
        for (auto& awaitable : awaitables_) {
            auto state = awaitable.poll(w);
            if (state.is_ready()) {
                if constexpr (std::is_void_v<result_t>) {
                    state.get_result();
                    return awaitable_state<result_type>::ready(std::make_tuple(result_t{}, i));
                } else {
                    return awaitable_state<result_type>::ready(
                        std::make_tuple(state.take_result(), i)
                    );
                }
            }
            i++;
        }
        return awaitable_state<result_type>::pending();
    }

  private:
    VecType& awaitables_;
};

template<typename VecType>
requires requires(VecType& v) { *std::begin(v); }
auto wait_first(VecType& awaitables) {
    return wait_first_iter_awaitable<VecType>(awaitables);
}

template<awaitable... Awaitables>
auto wait_first(Awaitables&&... awaitables) {
    return wait_first_awaitable<std::remove_cvref_t<Awaitables>...>(std::move(awaitables)...);
}

}  // namespace pollcoro
