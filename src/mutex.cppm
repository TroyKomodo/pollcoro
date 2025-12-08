module;

#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <atomic>

export module pollcoro:mutex;

import :awaitable;
import :is_blocking;
import :waker;

export namespace pollcoro {

class mutex;
class mutex_guard;
class mutex_lock_awaitable;

namespace detail {

struct waiter {
    waker waker_;
    std::atomic<bool> is_ready_{false};
};

struct mutex_state {
    mutable std::mutex mtx_;
    bool locked_{false};
    std::deque<std::shared_ptr<waiter>> waiters_;

    void release() {
        std::unique_lock lock(mtx_);
        if (!waiters_.empty()) {
            auto next = std::move(waiters_.front());
            waiters_.pop_front();
            next->is_ready_.store(true, std::memory_order_release);
            next->waker_.wake();
            lock.unlock();
        } else {
            locked_ = false;
        }
    }

    void remove_waiter(const std::shared_ptr<waiter>& waiter) {
        std::unique_lock lock(mtx_);
        auto it = std::find(waiters_.begin(), waiters_.end(), waiter);
        if (it != waiters_.end()) {
            waiters_.erase(it);
        }
    }
};

}  // namespace detail

/// RAII guard that holds a mutex lock. The lock is released when the guard is
/// destroyed or when `unlock()` is called explicitly.
class mutex_guard {
    detail::mutex_state* state_;

    explicit mutex_guard(detail::mutex_state* state)
        : state_(state) {}

    friend class mutex_lock_awaitable;
    friend class mutex;
  public:

    mutex_guard(mutex_guard&& other) noexcept : state_(std::exchange(other.state_, nullptr)) {}

    mutex_guard& operator=(mutex_guard&& other) noexcept {
        if (this != &other) {
            if (state_) {
                state_->release();
            }
            state_ = std::exchange(other.state_, nullptr);
        }
        return *this;
    }

    mutex_guard(const mutex_guard&) = delete;
    mutex_guard& operator=(const mutex_guard&) = delete;

    ~mutex_guard() {
        if (state_) {
            state_->release();
        }
    }

    /// Explicitly release the lock before the guard is destroyed.
    void unlock() {
        if (state_) {
            state_->release();
            state_ = nullptr;
        }
    }

    /// Check if this guard still holds the lock.
    explicit operator bool() const noexcept {
        return state_ != nullptr;
    }
};

/// Awaitable that acquires a mutex lock. Returns a `mutex_guard` when the lock
/// is acquired.
class mutex_lock_awaitable : public awaitable_always_blocks {
    detail::mutex_state* state_;
    std::shared_ptr<detail::waiter> waiter_ = std::make_shared<detail::waiter>();
    bool registered_{false};

    void deregister() {
        if (registered_ && state_ && waiter_) {
            if (waiter_->is_ready_.load(std::memory_order_acquire)) {
                state_->release();
            } else {
                state_->remove_waiter(waiter_);
                registered_ = false;
            }
        }
    }

    using result_type = mutex_guard;
    using state_type = awaitable_state<result_type>;

  public:
    explicit mutex_lock_awaitable(detail::mutex_state* state)
        : state_(state) {}

    mutex_lock_awaitable(mutex_lock_awaitable&& other) noexcept
        : state_(std::exchange(other.state_, nullptr)),
          waiter_(std::move(other.waiter_)),
          registered_(std::exchange(other.registered_, false)) {}

    mutex_lock_awaitable& operator=(mutex_lock_awaitable&& other) noexcept {
        if (this != &other) {
            deregister();
            state_ = std::exchange(other.state_, nullptr);
            waiter_ = std::move(other.waiter_);
            registered_ = std::exchange(other.registered_, false);
        }
        return *this;
    }

    mutex_lock_awaitable(const mutex_lock_awaitable&) = delete;
    mutex_lock_awaitable& operator=(const mutex_lock_awaitable&) = delete;

    ~mutex_lock_awaitable() {
        deregister();
    }

    state_type poll(const waker& w) {
        // Fast check if the waiter is ready
        if (waiter_->is_ready_.load(std::memory_order_acquire)) {
            return state_type::ready(mutex_guard(std::exchange(state_, nullptr)));
        }

        // Update waker
        std::unique_lock lock(state_->mtx_);
        waiter_->waker_ = w;

        if (!registered_) {
            if (!state_->locked_) {
                state_->locked_ = true;
                return state_type::ready(mutex_guard(std::exchange(state_, nullptr)));
            }
            
            registered_ = true;
            state_->waiters_.push_back(waiter_);
            return state_type::pending();
        }

        if (!waiter_->is_ready_) {
            return state_type::pending();
        }

        state_->locked_ = true;
        return state_type::ready(mutex_guard(std::exchange(state_, nullptr)));
    }
};

/// An async mutex that can be held across yield points.
///
/// Unlike `std::mutex`, this mutex is designed for cooperative async code where
/// tasks may yield while holding the lock. Waiters are queued in FIFO order
/// and woken when the lock becomes available.
///
/// Example:
/// ```cpp
/// pollcoro::mutex mtx;
///
/// task<void> example() {
///     auto guard = co_await mtx.lock();
///     // critical section - can yield here
///     co_await some_async_operation();
///     // guard automatically released when it goes out of scope
/// }
/// ```
class mutex {
    detail::mutex_state state_;

  public:
    mutex() = default;

    /// Returns an awaitable that acquires the lock. The returned awaitable
    /// yields a `mutex_guard` that releases the lock when destroyed.
    mutex_lock_awaitable lock() {
        return mutex_lock_awaitable(&state_);
    }

    /// Attempts to acquire the lock immediately without blocking.
    /// Returns a `mutex_guard` if successful, or `std::nullopt` if the lock
    /// is currently held.
    std::optional<mutex_guard> try_lock() {
        std::unique_lock lock(state_.mtx_);
        if (!state_.locked_) {
            state_.locked_ = true;
            return mutex_guard(&state_);
        }
        return std::nullopt;
    }

    /// Check if the mutex is currently locked. Note that this is only a
    /// snapshot and the state may change immediately after this call returns.
    bool is_locked() const {
        std::unique_lock lock(state_.mtx_);
        return state_.locked_;
    }
};

}  // namespace pollcoro

