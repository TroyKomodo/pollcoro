#pragma once

#include "export.hpp"

#include <cassert>
#include <memory>
#include <type_traits>

POLLCORO_EXPORT namespace pollcoro {
    class waker;  // Forward declaration

    struct waker_vtable {
        void (*wake)(void*);
        waker (*clone)(void*);
    };

    namespace detail {

    template<typename WakerType>
    void waker_wake_impl(void* data) noexcept {
        static_cast<WakerType*>(data)->wake();
    }

    template<typename WakerType>
    waker waker_clone_impl(void* data) noexcept;  // Forward declaration, defined after waker class

    template<typename WakerType, typename = void>
    struct waker_has_clone : std::false_type {};

    template<typename WakerType>
    struct waker_has_clone<WakerType, std::void_t<decltype(std::declval<WakerType>().clone())>>
        : std::true_type {};

    template<typename WakerType>
    inline constexpr waker_vtable make_waker_vtable() {
        return waker_vtable{
            .wake = detail::waker_wake_impl<WakerType>,
            .clone = []() {
                if constexpr (waker_has_clone<WakerType>::value) {
                    return detail::waker_clone_impl<WakerType>;
                } else {
                    return nullptr;
                }
            }(),
        };
    }

    template<typename WakerType>
    inline constexpr auto waker_vtable_v = make_waker_vtable<WakerType>();

    }  // namespace detail

    class waker {
        const waker_vtable* vtable_ = nullptr;
        std::unique_ptr<void, void (*)(void*)> data_ = {nullptr, nullptr};

        waker(const waker_vtable* vtable, void* data, void (*destroy)(void*)) noexcept
            : vtable_(vtable), data_(data, destroy) {}

      public:
        waker() = default;

        template<typename WakerType>
        waker(WakerType* ptr, void (*destroy)(WakerType*) = nullptr) noexcept
            : waker(
                  &detail::waker_vtable_v<WakerType>,
                  ptr,
                  reinterpret_cast<void (*)(void*)>(destroy)
              ) {}

        template<typename WakerType>
        waker(std::unique_ptr<WakerType> ptr) noexcept
            : waker(&detail::waker_vtable_v<WakerType>, ptr.release(), [](void* p) {
                  delete static_cast<WakerType*>(p);
              }) {}

        template<typename WakerType>
        waker(std::unique_ptr<WakerType, void (*)(WakerType*)> ptr) noexcept
            : waker(ptr.release(), ptr.get_deleter()) {}

        template<typename WakerType>
        waker(WakerType& ref) noexcept
            : waker(&detail::waker_vtable_v<WakerType>, &ref, [](void*) {}) {}

        void wake() noexcept {
            if (vtable_ && vtable_->wake) {
                vtable_->wake(data_.get());
            }
        }

        waker clone() noexcept {
            if (vtable_ && vtable_->clone) {
                return vtable_->clone(data_.get());
            } else {
                return waker(vtable_, data_.get(), [](void*) {});
            }
        }

        bool will_wake(const waker& other) const noexcept {
            return vtable_ && other.vtable_ && vtable_->wake == other.vtable_->wake &&
                data_.get() == other.data_.get();
        }
    };

    namespace detail {

    template<typename WakerType>
    waker waker_clone_impl(void* data) noexcept {
        return static_cast<WakerType*>(data)->clone();
    }

    }  // namespace detail

}  // namespace pollcoro
