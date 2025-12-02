#include "c_interop.h"

#include <pollcoro/task.hpp>
#include <pollcoro/yield.hpp>
#include <pollcoro/block_on.hpp>

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

future_poll_result_t future_poll(future_t* future, future_poll_waker_t waker, void* data) {
    auto w = pollcoro::waker(waker, data);
    return future->task.on_poll(w).is_ready() ? FUTURE_POLL_READY : FUTURE_POLL_PENDING;
}

void future_wait_until_ready(future_t* future) {
    pollcoro::block_on(future->task);
}

}