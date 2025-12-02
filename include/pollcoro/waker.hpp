#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <utility>
#endif

#include "export.hpp"

POLLCORO_EXPORT namespace pollcoro {

class waker {
    using waker_func_t = void (*)(void*);
    waker_func_t waker_ = nullptr;
    void* data_ = nullptr;

  public:
    waker() = default;

    template<typename Func, typename Data>
    waker(Func&& func, Data* data) {
        auto fn_ptr = static_cast<void (*)(Data*)>(+std::forward<Func>(func));
        waker_ = reinterpret_cast<waker_func_t>(fn_ptr);
        data_ = data;
    }

    void operator()() {
        if (waker_)
            waker_(data_);
    }

    operator bool() const {
        return waker_ != nullptr;
    }

    bool operator==(const waker& other) const {
        return waker_ == other.waker_ && data_ == other.data_;
    }

    bool operator!=(const waker& other) const {
        return !(*this == other);
    }
};

}  // namespace pollcoro
