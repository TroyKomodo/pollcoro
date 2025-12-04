#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <memory>
#include <utility>
#endif

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "stream_awaitable.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<typename T = void>
    class generic_awaitable : public awaitable_always_blocks {
        awaitable_state<T> (*poll_)(void* awaitable, const waker& w) = nullptr;
        std::unique_ptr<void, void (*)(void*)> awaitable_ = {nullptr, nullptr};

      public:
        template<POLLCORO_CONCEPT(awaitable) Awaitable>
        generic_awaitable(Awaitable awaitable) {
            POLLCORO_STATIC_ASSERT(Awaitable);

            awaitable_ = {
                static_cast<void*>(new Awaitable(std::move(awaitable))),
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

    template<typename T>
    class generic_stream_awaitable : public awaitable_always_blocks {
        stream_awaitable_state<T> (*poll_next_)(void* awaitable, const waker& w) = nullptr;
        std::unique_ptr<void, void (*)(void*)> awaitable_ = {nullptr, nullptr};

      public:
        template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
        generic_stream_awaitable(StreamAwaitable stream) {
            POLLCORO_STATIC_ASSERT_STREAM(StreamAwaitable);

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

    template<POLLCORO_CONCEPT(awaitable) Awaitable>
    auto generic(Awaitable awaitable) {
        POLLCORO_STATIC_ASSERT(Awaitable);

        return generic_awaitable<awaitable_result_t<Awaitable>>(std::move(awaitable));
    }

    template<POLLCORO_CONCEPT(stream_awaitable) StreamAwaitable>
    auto generic(StreamAwaitable stream) {
        POLLCORO_STATIC_ASSERT_STREAM(StreamAwaitable);

        return generic_stream_awaitable<stream_awaitable_result_t<StreamAwaitable>>(
            std::move(stream)
        );
    }

}  // namespace pollcoro