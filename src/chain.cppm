module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <cstddef>
#include <tuple>
#include <utility>
#endif

export module pollcoro:chain;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable... StreamAwaitables>
class chain_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitables...> {
    static_assert(sizeof...(StreamAwaitables) > 0, "chain requires at least one stream");

    std::tuple<StreamAwaitables...> streams_;
    size_t current_index_{0};

    using result_type =
        stream_awaitable_result_t<std::tuple_element_t<0, std::tuple<StreamAwaitables...>>>;
    using state_type = stream_awaitable_state<result_type>;

    template<size_t I>
    state_type poll_stream(const waker& w) {
        auto state = std::get<I>(streams_).poll_next(w);
        if (state.is_done()) {
            current_index_ = I + 1;
            if constexpr (I + 1 < sizeof...(StreamAwaitables)) {
                return poll_stream<I + 1>(w);
            }
            return state_type::done();
        }
        return state;
    }

    template<size_t... Is>
    state_type poll_impl(const waker& w, std::index_sequence<Is...>) {
        state_type result = state_type::done();
        std::ignore = ((current_index_ == Is ? (result = poll_stream<Is>(w), true) : false) || ...);
        return result;
    }

    template<stream_awaitable S, stream_awaitable... Ss>
    friend chain_stream_awaitable<std::remove_cvref_t<S>, Ss...>
    operator|(S&& stream, chain_stream_awaitable<Ss...>&& chain);

  public:
    chain_stream_awaitable(StreamAwaitables&&... streams) : streams_(std::move(streams)...) {}

    chain_stream_awaitable(std::tuple<StreamAwaitables...>&& streams)
        : streams_(std::move(streams)) {}

    state_type poll_next(const waker& w) {
        return poll_impl(w, std::make_index_sequence<sizeof...(StreamAwaitables)>{});
    }
};

template<stream_awaitable... StreamAwaitables>
chain_stream_awaitable(StreamAwaitables...) -> chain_stream_awaitable<StreamAwaitables...>;

template<stream_awaitable... StreamAwaitables>
chain_stream_awaitable(std::tuple<StreamAwaitables...>)
    -> chain_stream_awaitable<StreamAwaitables...>;

template<stream_awaitable... StreamAwaitables>
constexpr auto chain(StreamAwaitables&&... streams) {
    return chain_stream_awaitable<std::remove_cvref_t<StreamAwaitables>...>(std::move(streams)...);
}

template<stream_awaitable StreamAwaitable, stream_awaitable... StreamAwaitables>
chain_stream_awaitable<std::remove_cvref_t<StreamAwaitable>, StreamAwaitables...>
operator|(StreamAwaitable&& stream, chain_stream_awaitable<StreamAwaitables...>&& chain) {
    return chain_stream_awaitable<std::remove_cvref_t<StreamAwaitable>, StreamAwaitables...>(
        std::tuple_cat(std::make_tuple(std::move(stream)), std::move(chain.streams_))
    );
}
}  // namespace pollcoro
