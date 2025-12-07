module;

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

export module pollcoro:take_while;

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
namespace detail {
template<typename Predicate, typename T>
concept take_while_predicate = requires(Predicate predicate, const T& result) {
    { predicate(result) } -> std::same_as<bool>;
};
}  // namespace detail

template<
    stream_awaitable StreamAwaitable,
    detail::take_while_predicate<stream_awaitable_result_t<StreamAwaitable>> Predicate>
class take_while_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    std::decay_t<StreamAwaitable> stream_;
    std::decay_t<Predicate> predicate_;
    bool taking_{true};

    using result_type = stream_awaitable_result_t<StreamAwaitable>;
    using state_type = stream_awaitable_state<result_type>;

  public:
    take_while_stream_awaitable(StreamAwaitable&& stream, Predicate&& predicate)
        : stream_(std::forward<StreamAwaitable>(stream)),
          predicate_(std::forward<Predicate>(predicate)) {}

    state_type poll_next(const waker& w) {
        auto state = stream_.poll_next(w);
        if (state.is_done()) {
            return state_type::done();
        }
        if (state.is_ready() && taking_) {
            auto result = state.take_result();
            taking_ = std::invoke(predicate_, result);
            if (!taking_) {
                return state_type::done();
            }
            return state_type::ready(std::move(result));
        }
        return state;
    }
};

template<
    stream_awaitable StreamAwaitable,
    detail::take_while_predicate<stream_awaitable_result_t<StreamAwaitable>> Predicate>
constexpr auto take_while(StreamAwaitable&& stream, Predicate&& predicate) {
    return take_while_stream_awaitable<StreamAwaitable, Predicate>(
        std::forward<StreamAwaitable>(stream), std::forward<Predicate>(predicate)
    );
}

template<typename Predicate>
struct take_while_stream_composable {
    std::decay_t<Predicate> predicate_;
};

template<typename Predicate>
constexpr auto take_while(Predicate&& predicate) {
    return take_while_stream_composable<Predicate>(std::forward<Predicate>(predicate));
}

template<
    stream_awaitable StreamAwaitable,
    detail::take_while_predicate<stream_awaitable_result_t<StreamAwaitable>> Predicate>
auto operator|(StreamAwaitable&& stream, take_while_stream_composable<Predicate>&& composable) {
    return take_while_stream_awaitable<StreamAwaitable, Predicate>(
        std::forward<StreamAwaitable>(stream), std::forward<Predicate>(composable.predicate_)
    );
}
}  // namespace pollcoro
