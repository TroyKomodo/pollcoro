#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <cstdint>
#endif

#include "awaitable.hpp"
#include "export.hpp"
#include "is_blocking.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    class yield_awaitable : public awaitable_always_blocks {
        uint32_t ready_{0};

      public:
        yield_awaitable() : yield_awaitable(1) {}

        explicit yield_awaitable(uint32_t ready) : ready_(ready) {}

        awaitable_state<> poll(const waker& w) {
            if (ready_ > 0) {
                ready_--;
            }
            w.wake();
            return ready_ == 0 ? awaitable_state<>::ready() : awaitable_state<>::pending();
        }
    };

    inline auto yield(uint32_t ready = 1) {
        return yield_awaitable(ready);
    }

}  // namespace pollcoro
