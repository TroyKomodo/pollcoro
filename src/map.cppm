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
    std::decay_t<Awaitable> awaitable;
    std::decay_t<Func> func;

    using result_type = std::invoke_result_t<Func, awaitable_result_t<Awaitable>>;
    using state_type = awaitable_state<result_type>;

  public:
    map_awaitable(Awaitable&& awaitable, Func&& func)
        : awaitable(std::forward<Awaitable>(awaitable)), func(std::forward<Func>(func)) {}

    state_type poll(const waker& w) {
        return awaitable.poll(w).map(func);
    }
};

template<stream_awaitable StreamAwaitable, typename Func>
class map_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable stream_;
    std::decay_t<Func> func;

    using result_type = std::invoke_result_t<Func, stream_awaitable_result_t<StreamAwaitable>>;
    using state_type = stream_awaitable_state<result_type>;

  public:
    map_stream_awaitable(StreamAwaitable stream, Func&& func)
        : stream_(std::move(stream)), func(std::forward<Func>(func)) {}

    state_type poll_next(const waker& w) {
        return stream_.poll_next(w).map(func);
    }
};

template<typename Func>
class map_composable {
  public:
    std::decay_t<Func> func;

    map_composable(Func&& func) : func(std::forward<Func>(func)) {}
};

template<awaitable Awaitable, typename Func>
auto map(Awaitable&& awaitable, Func&& func) {
    return map_awaitable<Awaitable, Func>(
        std::forward<Awaitable>(awaitable), std::forward<Func>(func)
    );
}

template<stream_awaitable StreamAwaitable, typename Func>
auto map(StreamAwaitable&& stream, Func&& func) {
    return map_stream_awaitable<StreamAwaitable, Func>(
        std::forward<StreamAwaitable>(stream), std::forward<Func>(func)
    );
}

template<typename Func>
auto map(Func&& func) {
    return map_composable<Func>(std::forward<Func>(func));
}

template<awaitable Awaitable, typename Func>
auto operator|(Awaitable&& awaitable, map_composable<Func>&& mapper) {
    return map_awaitable<Awaitable, Func>(
        std::forward<Awaitable>(awaitable), std::forward<Func>(mapper.func)
    );
}

template<stream_awaitable StreamAwaitable, typename Func>
auto operator|(StreamAwaitable&& stream, map_composable<Func>&& mapper) {
    return map_stream_awaitable<StreamAwaitable, Func>(
        std::forward<StreamAwaitable>(stream), std::forward<Func>(mapper.func)
    );
}

}  // namespace pollcoro
