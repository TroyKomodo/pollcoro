module;

#include <utility>

export module pollcoro:repeat;

import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<typename T>
class repeat_stream_awaitable : public awaitable_never_blocks {
    T value_;
    using state_type = stream_awaitable_state<T>;

  public:
    repeat_stream_awaitable(T value) : value_(std::move(value)) {}

    state_type poll_next(const waker& w) {
        return state_type::ready(value_);
    }
};

template<typename T>
constexpr auto repeat(T value) {
    return repeat_stream_awaitable<T>(std::move(value));
}
}  // namespace pollcoro
