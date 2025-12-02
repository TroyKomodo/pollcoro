#pragma once

#include "export.hpp"
#include "concept.hpp"
#include "pollable_state.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {

template<POLLCORO_CONCEPT(awaitable) Awaitable>
class ref_awaitable {
    POLLCORO_STATIC_ASSERT(Awaitable);

    Awaitable& awaitable;

  public:
    ref_awaitable(Awaitable& awaitable) : awaitable(awaitable) {}

    pollable_state<awaitable_result_t<Awaitable>> on_poll(waker& w) {
        return awaitable.on_poll(w);
    }
};

template<POLLCORO_CONCEPT(awaitable) Awaitable>
auto ref(Awaitable& awaitable) {
    POLLCORO_STATIC_ASSERT(Awaitable);

    return ref_awaitable<Awaitable>(awaitable);
}

}  // namespace pollcoro