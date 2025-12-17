module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#endif

export module pollcoro:wait_all;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :awaitable;
import :is_blocking;
import :waker;

export namespace pollcoro {
namespace detail {

// Filter out void types from a parameter pack into a tuple
template<typename... Ts>
struct filter_void {
    using type = std::tuple<>;
};

template<typename T, typename... Rest>
struct filter_void<T, Rest...> {
  private:
    using rest_tuple = typename filter_void<Rest...>::type;

  public:
    using type = std::conditional_t<
        std::is_void_v<T> || std::is_same_v<T, std::monostate>,
        rest_tuple,
        decltype(std::tuple_cat(std::declval<std::tuple<T>>(), std::declval<rest_tuple>()))>;
};

template<typename... Ts>
using filter_void_t = typename filter_void<Ts...>::type;

// Get filtered result type for awaitables
template<awaitable... Awaitables>
using wait_all_result_t = filter_void_t<awaitable_result_t<Awaitables>...>;

template<typename Result>
struct wait_all_iter_result {
    using type = std::vector<Result>;
    std::map<std::size_t, Result, std::less<>> results_{};

    void insert(std::size_t index, Result result) {
        results_.emplace(index, std::move(result));
    }

    bool has_result(std::size_t index) const {
        return results_.find(index) != results_.end();
    }

    std::size_t size() const {
        return results_.size();
    }

    type build() {
        type results;
        results.reserve(results_.size());
        for (auto& [_, result] : results_) {
            results.emplace_back(std::move(result));
        }
        return results;
    }
};

template<>
struct wait_all_iter_result<void> {
    using type = void;
    std::map<std::size_t, std::monostate, std::less<>> results_{};

    void insert(std::size_t index) {
        results_.emplace(index, std::monostate{});
    }

    bool has_result(std::size_t index) const {
        return results_.find(index) != results_.end();
    }

    std::size_t size() const {
        return results_.size();
    }
};

}  // namespace detail

template<awaitable... Awaitables>
class wait_all_awaitable : public awaitable_maybe_blocks<Awaitables...> {
  public:
    using result_type = detail::wait_all_result_t<Awaitables...>;

    explicit wait_all_awaitable(Awaitables&&... awaitables)
        : awaitables_(std::move(awaitables)...) {}

    awaitable_state<result_type> poll(const waker& w) {
        bool all_ready = poll_all(w, std::index_sequence_for<Awaitables...>{});

        if (all_ready) {
            return awaitable_state<result_type>::ready(
                build_result(std::index_sequence_for<Awaitables...>{})
            );
        }
        return awaitable_state<result_type>::pending();
    }

  private:
    // Store results for each awaitable (using optional to track completion)
    // For void awaitables, we just store a bool
    template<typename T>
    using stored_result_t = std::conditional_t<
        std::is_void_v<awaitable_result_t<T>>,
        bool,  // just track completion for void
        std::optional<awaitable_result_t<T>>>;

    std::tuple<Awaitables...> awaitables_;
    std::tuple<stored_result_t<Awaitables>...> results_{};

    template<std::size_t I>
    bool poll_one(const waker& w) {
        auto& awaitable = std::get<I>(awaitables_);
        auto& stored = std::get<I>(results_);

        using awaitable_t = std::tuple_element_t<I, std::tuple<Awaitables...>>;
        using result_t = awaitable_result_t<awaitable_t>;

        // Check if already completed
        if constexpr (std::is_void_v<result_t>) {
            if (stored)
                return true;  // already done
        } else {
            if (stored.has_value())
                return true;  // already done
        }

        // Poll the awaitable
        auto state = awaitable.poll(w);
        if (state.is_ready()) {
            if constexpr (std::is_void_v<result_t>) {
                stored = true;
            } else {
                stored = state.take_result();
            }
            return true;
        }
        return false;
    }

    template<std::size_t... Is>
    bool poll_all(const waker& w, std::index_sequence<Is...>) {
        // Poll all and check if all are ready
        // Note: we poll ALL of them every time to register wakers
        bool results[] = {poll_one<Is>(w)...};
        return (results[Is] && ...);
    }

    // Build the filtered result tuple (excluding void results)
    template<std::size_t... Is>
    result_type build_result(std::index_sequence<Is...>) {
        return build_result_impl<Is...>();
    }

    template<std::size_t... Is>
    result_type build_result_impl() {
        return std::tuple_cat(get_single_result<Is>()...);
    }

    template<std::size_t I>
    auto get_single_result() {
        using awaitable_t = std::tuple_element_t<I, std::tuple<Awaitables...>>;
        using result_t = awaitable_result_t<awaitable_t>;

        if constexpr (std::is_void_v<result_t>) {
            // Void results are excluded from the tuple
            return std::tuple<>();
        } else {
            // Move the result out
            return std::make_tuple(std::move(*std::get<I>(results_)));
        }
    }
};

template<typename VecType>
requires requires(VecType& v) { *std::begin(v); }
class wait_all_iter_awaitable
    : public awaitable_maybe_blocks<decltype(*std::begin(std::declval<VecType&>()))> {
    using Awaitable = decltype(*std::begin(std::declval<VecType&>()));
    using result_storage_t = detail::wait_all_iter_result<awaitable_result_t<Awaitable>>;

  public:
    using result_type = typename result_storage_t::type;

    explicit wait_all_iter_awaitable(VecType& awaitables) : awaitables_(awaitables) {}

    awaitable_state<result_type> poll(const waker& w) {
        std::size_t i = 0;
        for (auto& awaitable : awaitables_) {
            if (results_.has_result(i)) {
                i++;
                continue;
            }

            auto state = awaitable.poll(w);
            if (state.is_ready()) {
                if constexpr (std::is_void_v<result_type>) {
                    results_.insert(i);
                } else {
                    results_.insert(i, state.take_result());
                }
            }
            i++;
        }
        if (results_.size() == awaitables_.size()) {
            if constexpr (std::is_void_v<result_type>) {
                return awaitable_state<result_type>::ready();
            } else {
                return awaitable_state<result_type>::ready(results_.build());
            }
        } else {
            return awaitable_state<result_type>::pending();
        }
    }

  private:
    VecType& awaitables_;
    result_storage_t results_{};
};

template<typename VecType>
requires requires(VecType& v) { *std::begin(v); }
auto wait_all(VecType& awaitables) {
    return wait_all_iter_awaitable<VecType>(awaitables);
}

template<awaitable... Awaitables>
auto wait_all(Awaitables&&... awaitables) {
    return wait_all_awaitable<std::remove_cvref_t<Awaitables>...>(std::move(awaitables)...);
}

}  // namespace pollcoro
