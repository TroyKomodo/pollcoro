#pragma once

#include "awaitable.hpp"
#include "export.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(awaitable) Awaitable>
    class ref_awaitable {
        POLLCORO_STATIC_ASSERT(Awaitable);

        Awaitable& awaitable;

      public:
        ref_awaitable(Awaitable& awaitable) : awaitable(awaitable) {}

        awaitable_state<awaitable_result_t<Awaitable>> poll(const waker& w) {
            return awaitable.poll(w);
        }
    };

    template<POLLCORO_CONCEPT(awaitable) Awaitable>
    auto ref(Awaitable & awaitable) {
        POLLCORO_STATIC_ASSERT(Awaitable);

        return ref_awaitable<Awaitable>(awaitable);
    }

}  // namespace pollcoro