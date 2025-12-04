#include "c_interop.h"

#include <stdio.h>
#include <stdbool.h>
#include <threads.h>

struct waker_data_t {
    mtx_t mutex;
    cnd_t condition;
    bool notified;
    int polls;
    int wakes;
};

void waker_wake(void* data) {
    struct waker_data_t* wd = (struct waker_data_t*)data;
    mtx_lock(&wd->mutex);
    printf("waker called wakes: %d\n", ++wd->wakes);
    wd->notified = true;
    cnd_signal(&wd->condition);
    mtx_unlock(&wd->mutex);
}

int main() {
    future_t* future;
    future_create(&future);

    struct waker_data_t wd;
    mtx_init(&wd.mutex, mtx_plain);
    cnd_init(&wd.condition);
    wd.notified = false;
    wd.polls = 0;
    wd.wakes = 0;

    future_poll_result_t result = FUTURE_POLL_PENDING;
    while (result == FUTURE_POLL_PENDING) {
        mtx_lock(&wd.mutex);
        wd.notified = false;
        mtx_unlock(&wd.mutex);
        waker_t waker = {&wd, waker_wake};
        printf("polling %d\n", ++wd.polls);
        result = future_poll(future, waker);

        mtx_lock(&wd.mutex);
        if (!wd.notified) {
            cnd_wait(&wd.condition, &wd.mutex);
        }
        mtx_unlock(&wd.mutex);
    }

    printf("future is ready after %d polls and %d wakes\n", wd.polls, wd.wakes);

    future_destroy(future);
    mtx_destroy(&wd.mutex);
    cnd_destroy(&wd.condition);

    return 0;
}