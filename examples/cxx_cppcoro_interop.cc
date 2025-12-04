#include "c_interop.h"

#undef linux

#include <cppcoro/async_auto_reset_event.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <iostream>
#include <memory>
#include <thread>

class future_container {
    struct deleter {
        void operator()(future_t* future) {
            future_destroy(future);
        }
    };

    std::unique_ptr<future_t, deleter> future_ = nullptr;

  public:
    future_container() {
        future_t* future;
        future_create(&future);
        future_ = {future, deleter()};
    }

    void wait_until_ready() {
        future_wait_until_ready(future_.get());
    }

    // Note: We need to pass an io_service here as this is how cppcoro
    // Drives the event loop.
    auto operator()(cppcoro::io_service& io_service) noexcept -> cppcoro::task<void> {
        cppcoro::async_auto_reset_event event;

        struct waker_data_t {
            cppcoro::async_auto_reset_event& event;
        };

        waker_data_t wd{event};

        auto waker = waker_t{
            .data = &wd,
            .wake_function = [](void* data) {
                static_cast<waker_data_t*>(data)->event.set();
            },
        };

        std::cout << "c++ future is not ready " << std::this_thread::get_id() << std::endl;
        size_t poll_count = 0;

        while (true) {
            std::cout << "c++ polling " << std::this_thread::get_id() << " " << ++poll_count
                      << std::endl;
            auto result = future_poll(future_.get(), waker);

            if (result == FUTURE_POLL_READY) {
                std::cout << "c++ future is ready after " << poll_count << " polls "
                          << std::this_thread::get_id() << std::endl;
                co_return;
            } else {
                co_await event;
                // This schedule here puts us back on the event loop.
                // Since normally, what happens is that when the event is set,
                // We get resumed INSIDE the callback (resume vs poll difference)
                // So waiting here for the schedule puts us back into the event loop.
                co_await io_service.schedule();
            }
        }
    }
};

cppcoro::task<int> async_main(cppcoro::io_service& io_service) {
    cppcoro::io_work_scope work_scope(io_service);
    auto future = future_container();
    co_await future(io_service);
    co_return 0;
}

cppcoro::task<void> drive_io(cppcoro::io_service& io_service) {
    io_service.process_events();
    co_return;
}

int main() {
    cppcoro::io_service io_service;
    return std::get<0>(
        cppcoro::sync_wait(cppcoro::when_all(async_main(io_service), drive_io(io_service)))
    );
}
