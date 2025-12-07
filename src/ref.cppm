module;

export module pollcoro:ref;

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<awaitable Awaitable>
class ref_awaitable : public awaitable_maybe_blocks<Awaitable> {
    Awaitable& awaitable;

  public:
    ref_awaitable(Awaitable& awaitable) : awaitable(awaitable) {}

    awaitable_state<awaitable_result_t<Awaitable>> poll(const waker& w) {
        return awaitable.poll(w);
    }
};

template<stream_awaitable StreamAwaitable>
class ref_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable& stream;

  public:
    ref_stream_awaitable(StreamAwaitable& stream) : stream(stream) {}

    stream_awaitable_state<stream_awaitable_result_t<StreamAwaitable>> poll_next(const waker& w) {
        return stream.poll_next(w);
    }
};

template<awaitable Awaitable>
auto ref(Awaitable& awaitable) {
    return ref_awaitable<Awaitable>(awaitable);
}

template<stream_awaitable StreamAwaitable>
auto ref(StreamAwaitable& stream) {
    return ref_stream_awaitable<StreamAwaitable>(stream);
}
}  // namespace pollcoro
