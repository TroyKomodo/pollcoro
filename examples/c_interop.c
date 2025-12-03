#include "c_interop.h"

#include <stdio.h>
#include <stdbool.h>
#include <threads.h>

struct waker_data_t {
    mtx_t mutex;
    cnd_t condition;
    bool notified;
};

void waker_wake(void* data) {
    printf("waker called\n");
    struct waker_data_t* wd = (struct waker_data_t*)data;
    mtx_lock(&wd->mutex);
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
    wd.notified = true;

    struct waker_vtable_t waker_vtable = {waker_wake, NULL, NULL};

    int polls = 0;
    future_poll_result_t result = FUTURE_POLL_PENDING;
    while (result == FUTURE_POLL_PENDING) {
        mtx_lock(&wd.mutex);
        if (wd.notified) {
            wd.notified = false;
            mtx_unlock(&wd.mutex);
            printf("polling %d\n", polls++);
            raw_waker_t waker = {&wd, &waker_vtable};
            result = future_poll(future, waker);
            mtx_lock(&wd.mutex);
        }

        if (!wd.notified) {
            cnd_wait(&wd.condition, &wd.mutex);
        }
        mtx_unlock(&wd.mutex);
    }

    printf("future is ready after %d polls\n", polls);

    future_destroy(future);
    mtx_destroy(&wd.mutex);
    cnd_destroy(&wd.condition);

    return 0;
}