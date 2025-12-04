#pragma once

#include "export.hpp"

POLLCORO_EXPORT namespace pollcoro {
    class waker;  // Forward declaration

    namespace detail {

    template<typename WakerType>
    inline void waker_wake_function(void* data) noexcept {
        static_cast<WakerType*>(data)->wake();
    }

    }  // namespace detail

    class waker {
        void (*wake_function_)(void*) = nullptr;
        void* data_ = nullptr;

      public:
        waker() = default;

        waker(void* data, void (*wake_function)(void*)) noexcept
            : wake_function_(wake_function), data_(data) {}

        template<typename WakerType>
        waker(WakerType& ref) noexcept : waker(&ref) {}

        template<typename WakerType>
        waker(WakerType* waker_ptr) noexcept
            : wake_function_(&detail::waker_wake_function<WakerType>),
              data_(static_cast<void*>(waker_ptr)) {}

        void wake() const noexcept {
            if (wake_function_) {
                wake_function_(data_);
            }
        }

        bool will_wake(const waker& other) const noexcept {
            return data_ == other.data_ && wake_function_ == other.wake_function_;
        }
    };
}  // namespace pollcoro
