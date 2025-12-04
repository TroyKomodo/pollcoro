#pragma once

#include "concept.hpp"
#include "export.hpp"
#include "pollable_state.hpp"
#include "waker.hpp"

#ifndef POLLCORO_MODULE_EXPORT
#include <functional>
#include <memory>
#include <mutex>
#endif

POLLCORO_EXPORT namespace pollcoro {
#if POLLCORO_USE_CONCEPTS
    template<typename Timer>
    concept timer = requires(Timer timer) {
        typename Timer::duration;
        typename Timer::time_point;
        { timer.now() } -> std::same_as<typename Timer::time_point>;
        {
            timer.now() + std::declval<typename Timer::duration>()
        } -> std::same_as<typename Timer::time_point>;
        {
            timer.register_callback(
                std::declval<const typename Timer::time_point&>(),
                std::declval<std::function<void()>>()
            )
        } -> std::same_as<void>;
    };
#endif

    template<POLLCORO_CONCEPT(timer) Timer>
    class sleep_awaitable {
        struct shared {
            std::mutex mutex;
            pollcoro::waker waker;
        };

        std::shared_ptr<shared> shared_;
        bool started_ = false;
        std::decay_t<Timer> timer_;
        typename Timer::time_point deadline_;

        void reset() {
            if (shared_ && started_) {
                std::lock_guard lock(shared_->mutex);
                shared_->waker = pollcoro::waker();
            }
            shared_ = nullptr;
            started_ = false;
        }

      public:
        sleep_awaitable(typename Timer::time_point deadline, Timer&& timer)
            : timer_(std::forward<Timer>(timer)),
              deadline_(deadline),
              shared_(std::make_shared<shared>()) {}

        ~sleep_awaitable() {
            reset();
        }

        sleep_awaitable(const sleep_awaitable& other) = delete;
        sleep_awaitable& operator=(const sleep_awaitable& other) = delete;

        sleep_awaitable(sleep_awaitable&& other) noexcept {
            reset();
            shared_ = std::move(other.shared_);
            started_ = other.started_;
            deadline_ = other.deadline_;
            timer_ = std::move(other.timer_);
            other.shared_ = nullptr;
            other.started_ = false;
        }

        sleep_awaitable& operator=(sleep_awaitable&& other) noexcept {
            if (this != &other) {
                reset();
                shared_ = std::move(other.shared_);
                started_ = other.started_;
                deadline_ = other.deadline_;
                timer_ = std::move(other.timer_);
                other.shared_ = nullptr;
                other.started_ = false;
            }
            return *this;
        }

        pollable_state<void> on_poll(const waker& w) {
            if (timer_.now() >= deadline_) {
                return pollable_state<void>::ready();
            }

            std::lock_guard lock(shared_->mutex);
            shared_->waker = w;
            if (!started_) {
                started_ = true;
                timer_.register_callback(deadline_, [shared = shared_]() {
                    std::lock_guard lock(shared->mutex);
                    shared->waker.wake();
                });
            }

            return pollable_state<void>::pending();
        }
    };

    template<POLLCORO_CONCEPT(timer) Timer>
    auto sleep_for(typename Timer::duration duration) {
        auto timer = Timer();
        auto deadline = timer.now() + duration;
        return sleep_awaitable<Timer>(deadline, std::move(timer));
    }

    template<POLLCORO_CONCEPT(timer) Timer>
    auto sleep_until(typename Timer::time_point deadline) {
        return sleep_awaitable<Timer>(deadline, Timer());
    }

    template<POLLCORO_CONCEPT(timer) Timer>
    auto sleep_for(typename Timer::duration duration, Timer && timer) {
        auto deadline = timer.now() + duration;
        return sleep_awaitable<Timer>(deadline, std::forward<Timer>(timer));
    }

    template<POLLCORO_CONCEPT(timer) Timer>
    auto sleep_until(typename Timer::time_point deadline, Timer && timer) {
        return sleep_awaitable<Timer>(deadline, std::forward<Timer>(timer));
    }
}  // namespace pollcoro