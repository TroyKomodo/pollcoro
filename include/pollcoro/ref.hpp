#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(awaitable) Awaitable>
    class ref_awaitable : public awaitable_maybe_blocks<Awaitable> {
        POLLCORO_STATIC_ASSERT(Awaitable);

        Awaitable& awaitable;

      public:
        ref_awaitable(Awaitable& awaitable) : awaitable(awaitable) {}

        awaitable_state<awaitable_result_t<Awaitable>> poll(const waker& w) {
            return awaitable.poll(w);
        }
    };

    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    class ref_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
        POLLCORO_STATIC_ASSERT_STREAM(StreamAwaitable);

        StreamAwaitable& stream;

      public:
        ref_stream_awaitable(StreamAwaitable& stream) : stream(stream) {}
    };

    template<POLLCORO_CONCEPT(awaitable) Awaitable>
    auto ref(Awaitable & awaitable) {
        POLLCORO_STATIC_ASSERT(Awaitable);
        return ref_awaitable<Awaitable>(awaitable);
    }

    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    auto ref(StreamAwaitable & stream) {
        POLLCORO_STATIC_ASSERT_STREAM(StreamAwaitable);
        return ref_stream_awaitable<StreamAwaitable>(stream);
    }
}  // namespace pollcoro
