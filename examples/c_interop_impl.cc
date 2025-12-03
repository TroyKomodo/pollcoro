#include "c_interop.h"

#include <pollcoro/block_on.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/waker.hpp>
#include <pollcoro/yield.hpp>

struct future_t {
    pollcoro::task<> task;
};

pollcoro::task<> task() {
    co_await pollcoro::yield(10);
    co_return;
}

extern "C" {

void future_create(future_t** future) {
    *future = new future_t(task());
}

void future_destroy(future_t* future) {
    delete future;
}

future_poll_result_t future_poll(future_t* future, raw_waker_t raw_waker) {
    struct custom_waker_t : raw_waker_t {
        ~custom_waker_t() {
            if (vtable->destroy) {
                vtable->destroy(data);
            }
        }

        custom_waker_t(raw_waker_t raw_waker) {
            vtable = raw_waker.vtable;
            data = raw_waker.data;
        }

        custom_waker_t(const custom_waker_t& other) = delete;
        custom_waker_t& operator=(const custom_waker_t& other) = delete;

        custom_waker_t(custom_waker_t&& other) noexcept {
            vtable = other.vtable;
            data = other.data;
            other.vtable = nullptr;
            other.data = nullptr;
        }

        custom_waker_t& operator=(custom_waker_t&& other) noexcept {
            if (this != &other) {
                if (vtable->destroy) {
                    vtable->destroy(data);
                }
                vtable = other.vtable;
                data = other.data;
                other.vtable = nullptr;
                other.data = nullptr;
            }
            return *this;
        }

        void wake() {
            if (vtable->wake) {
                vtable->wake(data);
            }
        }

        pollcoro::waker clone() {
            if (vtable->clone) {
                return pollcoro::waker(std::make_unique<custom_waker_t>(vtable->clone(data)));
            } else {
                return pollcoro::waker(this, +[](custom_waker_t*) {});
            }
        }
    } waker = {raw_waker};

    auto w = pollcoro::waker(waker);
    return future->task.on_poll(w).is_ready() ? FUTURE_POLL_READY : FUTURE_POLL_PENDING;
}

void future_wait_until_ready(future_t* future) {
    pollcoro::block_on(future->task);
}
}