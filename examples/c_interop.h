#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct future_t future_t;

typedef void (*future_poll_waker_t)(void* data);

typedef enum {
    FUTURE_POLL_READY = 0,
    FUTURE_POLL_PENDING = 1,
} future_poll_result_t;

struct waker_vtable_t;

typedef struct {
    void* data;
    void (*wake_function)(void* data);
} waker_t;

void future_create(future_t** future);
void future_destroy(future_t* future);
future_poll_result_t future_poll(future_t* future, waker_t waker);
void future_wait_until_ready(future_t* future);

#ifdef __cplusplus
}
#endif
