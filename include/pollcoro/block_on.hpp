#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <condition_variable>
#include <mutex>
#endif

#include "concept.hpp"
#include "export.hpp"
#include "waker.hpp"

POLLCORO_EXPORT namespace pollcoro {
    template<POLLCORO_CONCEPT(awaitable) Awaitable>
    auto block_on(Awaitable && awaitable) -> awaitable_result_t<Awaitable> {
        POLLCORO_STATIC_ASSERT(Awaitable);

        struct waker_data_t {
            std::mutex mutex;
            std::condition_variable cv;
            bool notified = false;

            void wake() noexcept {
                {
                    std::lock_guard lock(mutex);
                    notified = true;
                }
                cv.notify_all();
            }
        };

        waker_data_t wd;
        while (true) {
            std::unique_lock lock(wd.mutex);
            wd.notified = false;
            lock.unlock();

            auto result = awaitable.on_poll(waker(wd));
            if (result.is_ready()) {
                return result.take_result();
            }

            lock.lock();
            wd.cv.wait(lock, [&] {
                return wd.notified;
            });
        }
    }

}  // namespace pollcoro
