module;

export module pollcoro:pending;

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<typename T>
class pending_awaitable : public awaitable_always_blocks {
  public:
    awaitable_state<T> poll(const waker& w) {
        return awaitable_state<T>::pending();
    }
};

template<typename T>
class pending_stream_awaitable : public awaitable_always_blocks {
    using state_type = stream_awaitable_state<T>;

  public:
    state_type poll_next(const waker& w) {
        return state_type::pending();
    }
};

template<typename T>
constexpr auto pending() {
    return pending_awaitable<T>();
}

template<typename T>
constexpr auto pending_stream() {
    return pending_stream_awaitable<T>();
}
}  // namespace pollcoro
