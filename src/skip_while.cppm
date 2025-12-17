module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#endif

export module pollcoro:skip_while;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
namespace detail {
template<typename Predicate, typename T>
concept skip_while_predicate = requires(Predicate predicate, const T& result) {
    { predicate(result) } -> std::same_as<bool>;
};
}  // namespace detail

template<
    stream_awaitable StreamAwaitable,
    detail::skip_while_predicate<stream_awaitable_result_t<StreamAwaitable>> Predicate>
class skip_while_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable stream_;
    Predicate predicate_;
    bool skipping_{true};

    using result_type = stream_awaitable_result_t<StreamAwaitable>;
    using state_type = stream_awaitable_state<result_type>;

  public:
    skip_while_stream_awaitable(StreamAwaitable&& stream, Predicate&& predicate)
        : stream_(std::move(stream)), predicate_(std::move(predicate)) {}

    state_type poll_next(const waker& w) {
        while (true) {
            auto state = stream_.poll_next(w);
            if (state.is_done()) {
                return state_type::done();
            }
            if (state.is_ready() && skipping_) {
                auto result = state.take_result();
                skipping_ = std::invoke(predicate_, result);
                if (skipping_) {
                    continue;
                }
                return state_type::ready(std::move(result));
            }
            return state;
        }
    }
};

template<
    stream_awaitable StreamAwaitable,
    detail::skip_while_predicate<stream_awaitable_result_t<std::remove_cvref_t<StreamAwaitable>>>
        Predicate>
constexpr auto skip_while(StreamAwaitable&& stream, Predicate&& predicate) {
    return skip_while_stream_awaitable<
        std::remove_cvref_t<StreamAwaitable>,
        std::remove_cvref_t<Predicate>>(std::move(stream), std::move(predicate));
}

template<typename Predicate>
struct skip_while_stream_composable {
    Predicate predicate_;
};

template<
    stream_awaitable StreamAwaitable,
    detail::skip_while_predicate<stream_awaitable_result_t<std::remove_cvref_t<StreamAwaitable>>>
        Predicate>
auto operator|(StreamAwaitable&& stream, skip_while_stream_composable<Predicate>&& composable) {
    return skip_while_stream_awaitable<std::remove_cvref_t<StreamAwaitable>, Predicate>(
        std::move(stream), std::move(composable.predicate_)
    );
}

template<typename Predicate>
constexpr auto skip_while(Predicate&& predicate) {
    return skip_while_stream_composable<std::remove_cvref_t<Predicate>>(std::move(predicate));
}
}  // namespace pollcoro
