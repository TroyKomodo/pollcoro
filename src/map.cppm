module;

#include <type_traits>
#include <utility>

export module pollcoro:map;

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<awaitable Awaitable, typename Func>
class map_awaitable : public awaitable_maybe_blocks<Awaitable> {
    Awaitable awaitable;
    Func func;

    using result_type = std::invoke_result_t<Func, awaitable_result_t<Awaitable>>;
    using state_type = awaitable_state<result_type>;

  public:
    map_awaitable(Awaitable awaitable, Func func)
        : awaitable(std::move(awaitable)), func(std::move(func)) {}

    state_type poll(const waker& w) {
        return awaitable.poll(w).map(func);
    }
};

template<stream_awaitable StreamAwaitable, typename Func>
class map_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable stream_;
    Func func;

    using result_type = std::invoke_result_t<Func, stream_awaitable_result_t<StreamAwaitable>>;
    using state_type = stream_awaitable_state<result_type>;

  public:
    map_stream_awaitable(StreamAwaitable stream, Func func)
        : stream_(std::move(stream)), func(std::move(func)) {}

    state_type poll_next(const waker& w) {
        return stream_.poll_next(w).map(func);
    }
};

template<typename Func>
class map_composable {
  public:
    Func func;

    map_composable(Func func) : func(std::move(func)) {}
};

template<awaitable Awaitable, typename Func>
auto map(Awaitable awaitable, Func func) {
    return map_awaitable<Awaitable, Func>(std::move(awaitable), std::move(func));
}

template<stream_awaitable StreamAwaitable, typename Func>
auto map(StreamAwaitable stream, Func func) {
    return map_stream_awaitable<StreamAwaitable, Func>(std::move(stream), std::move(func));
}

template<typename Func>
auto map(Func func) {
    return map_composable<Func>(std::move(func));
}

template<awaitable Awaitable, typename Func>
auto operator|(Awaitable awaitable, map_composable<Func> mapper) {
    return map_awaitable<Awaitable, Func>(std::move(awaitable), std::move(mapper.func));
}

template<stream_awaitable StreamAwaitable, typename Func>
auto operator|(StreamAwaitable stream, map_composable<Func> mapper) {
    return map_stream_awaitable<StreamAwaitable, Func>(std::move(stream), std::move(mapper.func));
}

}  // namespace pollcoro
