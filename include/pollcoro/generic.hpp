#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <memory>
#include <utility>
#endif

#include "export.hpp"
#include "concept.hpp"
#include "pollable_state.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {

template<typename T = void>
class generic_awaitable {
    pollable_state<T> (*on_poll_)(void* awaitable, waker& w) = nullptr;
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
        on_poll_ = [](void* awaitable_ptr, waker& w) {
            return static_cast<Awaitable*>(awaitable_ptr)->on_poll(w);
        };
    }

    pollable_state<T> on_poll(waker& w) {
        return on_poll_(awaitable_.get(), w);
    }
};

template<POLLCORO_CONCEPT(awaitable) Awaitable>
auto generic(Awaitable awaitable) {
    POLLCORO_STATIC_ASSERT(Awaitable);

    return generic_awaitable<awaitable_result_t<Awaitable>>(std::move(awaitable));
}

}  // namespace pollcoro