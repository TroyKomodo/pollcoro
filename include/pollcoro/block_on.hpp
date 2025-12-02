#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <condition_variable>
#include <mutex>
#endif

#include "export.hpp"
#include "concept.hpp"
#include "waker.hpp"


POLLCORO_EXPORT namespace pollcoro {

template<POLLCORO_CONCEPT(awaitable) Awaitable>
auto block_on(Awaitable&& awaitable) -> awaitable_result_t<Awaitable> {
    POLLCORO_STATIC_ASSERT(Awaitable);

    struct waker_data_t {
        std::mutex mutex;
        std::condition_variable cv;
        bool notified = false;
    };

    waker_data_t wd{};
    while (true) {
        wd.notified = false;
        auto w = waker(
            [](waker_data_t* data) {
                {
                    std::lock_guard lock(data->mutex);
                    data->notified = true;
                }
                data->cv.notify_all();
            },
            &wd
        );
        auto result = awaitable.on_poll(w);
        if (result.is_ready()) {
            return result.take_result();
        }

        std::unique_lock lock(wd.mutex);
        wd.cv.wait(lock, [&] {
            return wd.notified;
        });
    }
}

}  // namespace pollcoro
