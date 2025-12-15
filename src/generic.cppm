module;

#include <memory>
#include <utility>

export module pollcoro:generic;

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<typename T = void>
class generic_awaitable : public awaitable_always_blocks {
    awaitable_state<T> (*poll_)(void* awaitable, const waker& w) = nullptr;
    std::unique_ptr<void, void (*)(void*)> awaitable_ = {nullptr, nullptr};

  public:
    template<awaitable Awaitable>
    generic_awaitable(Awaitable&& awaitable) {
        awaitable_ = {
            static_cast<void*>(new Awaitable(std::forward<Awaitable>(awaitable))),
            [](void* awaitable_ptr) {
                delete static_cast<Awaitable*>(awaitable_ptr);
            },
        };
        poll_ = [](void* awaitable_ptr, const waker& w) {
            return static_cast<Awaitable*>(awaitable_ptr)->poll(w);
        };
    }

    awaitable_state<T> poll(const waker& w) {
        return poll_(awaitable_.get(), w);
    }
};

template<awaitable Awaitable>
generic_awaitable(Awaitable) -> generic_awaitable<awaitable_result_t<Awaitable>>;

template<typename T>
class generic_stream_awaitable : public awaitable_always_blocks {
    stream_awaitable_state<T> (*poll_next_)(void* awaitable, const waker& w) = nullptr;
    std::unique_ptr<void, void (*)(void*)> awaitable_ = {nullptr, nullptr};

  public:
    template<stream_awaitable StreamAwaitable>
    generic_stream_awaitable(StreamAwaitable&& stream) {
        awaitable_ = {
            static_cast<void*>(new StreamAwaitable(std::move(stream))),
            [](void* stream_ptr) {
                delete static_cast<StreamAwaitable*>(stream_ptr);
            },
        };
        poll_next_ = [](void* stream_ptr, const waker& w) {
            return static_cast<StreamAwaitable*>(stream_ptr)->poll_next(w);
        };
    }

    stream_awaitable_state<T> poll_next(const waker& w) {
        return poll_next_(awaitable_.get(), w);
    }
};

template<stream_awaitable StreamAwaitable>
generic_stream_awaitable(StreamAwaitable)
    -> generic_stream_awaitable<stream_awaitable_result_t<StreamAwaitable>>;

template<awaitable Awaitable>
auto generic(Awaitable&& awaitable) {
    return generic_awaitable(std::move(awaitable));
}

template<stream_awaitable StreamAwaitable>
auto generic(StreamAwaitable&& stream) {
    return generic_stream_awaitable(std::move(stream));
}

}  // namespace pollcoro
