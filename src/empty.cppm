module;

export module pollcoro:empty;

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<typename T>
class empty_stream_awaitable : public awaitable_never_blocks {
    using state_type = stream_awaitable_state<T>;

  public:
    state_type poll_next(const waker& w) {
        return state_type::done();
    }
};

template<typename T>
constexpr auto empty() {
    return empty_stream_awaitable<T>();
}
}  // namespace pollcoro
