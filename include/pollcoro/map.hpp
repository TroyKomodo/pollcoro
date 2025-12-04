#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <utility>
#endif

#include "concept.hpp"
#include "export.hpp"
#include "pollable_state.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(awaitable) Awaitable, typename Func>
    class map_awaitable {
        POLLCORO_STATIC_ASSERT(Awaitable);

        Awaitable awaitable;
        std::decay_t<Func> func;

        using result_type = std::invoke_result_t<Func, awaitable_result_t<Awaitable>>;

      public:
        map_awaitable(Awaitable awaitable, Func&& func)
            : awaitable(std::move(awaitable)), func(std::forward<Func>(func)) {}

        pollable_state<result_type> on_poll(const waker& w) {
            return awaitable.on_poll(w).map(func);
        }
    };

    template<typename Func>
    class map_composable {
      public:
        std::decay_t<Func> func;

        map_composable(Func&& func) : func(std::forward<Func>(func)) {}
    };

    template<POLLCORO_CONCEPT(awaitable) Awaitable, typename Func>
    auto map(Awaitable awaitable, Func && func) {
        POLLCORO_STATIC_ASSERT(Awaitable);
        return map_awaitable<Awaitable, Func>(std::move(awaitable), std::forward<Func>(func));
    }

    template<typename Func>
    auto map(Func && func) {
        return map_composable<Func>(std::forward<Func>(func));
    }

    template<POLLCORO_CONCEPT(awaitable) Awaitable, typename Func>
    auto operator|(Awaitable&& awaitable, map_composable<Func>&& mapper) {
        POLLCORO_STATIC_ASSERT(Awaitable);
        return map_awaitable<Awaitable, Func>(std::move(awaitable), std::move(mapper.func));
    }

}  // namespace pollcoro
