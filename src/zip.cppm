module;

#include <cstddef>
#include <optional>
#include <tuple>
#include <utility>

export module pollcoro:zip;

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<stream_awaitable... StreamAwaitables>
class zip_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitables...> {
    std::tuple<StreamAwaitables...> streams_;
    std::tuple<std::optional<stream_awaitable_result_t<StreamAwaitables>>...> buffered_;

    using result_type = std::tuple<stream_awaitable_result_t<StreamAwaitables>...>;
    using state_type = stream_awaitable_state<result_type>;

    template<stream_awaitable S, stream_awaitable... Ss>
    friend zip_stream_awaitable<std::remove_cvref_t<S>, Ss...>
    operator|(S&& stream, zip_stream_awaitable<Ss...>&& zip);

    template<size_t... Is>
    state_type poll_impl(const waker& w, std::index_sequence<Is...>) {
        bool any_pending = false;
        bool any_done = false;

        auto poll_one = [&]<size_t I>(std::integral_constant<size_t, I>) {
            // Already have a buffered value for this stream
            if (std::get<I>(buffered_).has_value()) {
                return;
            }

            auto state = std::get<I>(streams_).poll_next(w);
            if (state.is_done()) {
                any_done = true;
            } else if (state.is_ready()) {
                std::get<I>(buffered_) = state.take_result();
            } else {
                any_pending = true;
            }
        };

        (poll_one(std::integral_constant<size_t, Is>{}), ...);

        if (any_done) {
            return state_type::done();
        }
        if (any_pending) {
            return state_type::pending();
        }

        // All ready - extract buffered values and clear
        result_type result{std::move(*std::get<Is>(buffered_))...};
        ((std::get<Is>(buffered_).reset()), ...);
        return state_type::ready(std::move(result));
    }

  public:
    zip_stream_awaitable(StreamAwaitables&&... streams) : streams_(std::move(streams)...) {}

    explicit zip_stream_awaitable(std::tuple<StreamAwaitables...>&& streams)
        : streams_(std::move(streams)) {}

    state_type poll_next(const waker& w) {
        return poll_impl(w, std::index_sequence_for<StreamAwaitables...>{});
    }
};

template<stream_awaitable... StreamAwaitables>
constexpr auto zip(StreamAwaitables&&... streams) {
    return zip_stream_awaitable<std::remove_cvref_t<StreamAwaitables>...>(std::move(streams)...);
}

template<stream_awaitable StreamAwaitable, stream_awaitable... StreamAwaitables>
zip_stream_awaitable<std::remove_cvref_t<StreamAwaitable>, StreamAwaitables...>
operator|(StreamAwaitable&& stream, zip_stream_awaitable<StreamAwaitables...>&& zip) {
    return zip_stream_awaitable<std::remove_cvref_t<StreamAwaitable>, StreamAwaitables...>(
        std::tuple_cat(std::make_tuple(std::move(stream)), std::move(zip.streams_))
    );
}
}  // namespace pollcoro
