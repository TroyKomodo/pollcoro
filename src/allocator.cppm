module;

#include <concepts>
#include <cstddef>
#include <functional>
#include <new>

export module pollcoro:allocator;

import :awaitable;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
template<typename Impl>
concept allocator_impl = requires(Impl& impl, size_t size, void* ptr) {
    { impl.allocate(size) } -> std::same_as<void*>;
    { impl.deallocate(ptr) } -> std::same_as<void>;
};

namespace detail {
template<allocator_impl Impl>
void* allocate_impl(void* instance, size_t size) {
    return static_cast<Impl*>(instance)->allocate(size);
}

template<allocator_impl Impl>
void deallocate_impl(void* instance, void* ptr) noexcept {
    return static_cast<Impl*>(instance)->deallocate(ptr);
}
}  // namespace detail

class allocator {
    using allocate_fn = void* (*)(void* instance, size_t size);
    using deallocate_fn = void (*)(void* instance, void* ptr) noexcept;
    void* instance_ = nullptr;
    allocate_fn allocate_ = nullptr;
    deallocate_fn deallocate_ = nullptr;

  public:
    explicit allocator(void* instance, allocate_fn allocate, deallocate_fn deallocate)
        : instance_(instance), allocate_(allocate), deallocate_(deallocate) {}

    template<allocator_impl Impl>
    allocator(Impl& impl)
        : instance_(&impl),
          allocate_(detail::allocate_impl<Impl>),
          deallocate_(detail::deallocate_impl<Impl>) {}

    template<typename Func, typename... Args>
    auto in_scope(Func&& func, Args&&... args) const;

    void* allocate(size_t size) const {
        return allocate_(instance_, size);
    }

    void deallocate(void* ptr) const noexcept {
        deallocate_(instance_, ptr);
    }
};

inline allocator default_allocator(
    nullptr,
    +[](void* instance, size_t size) {
        return ::operator new(size);
    },
    +[](void* instance, void* ptr) noexcept {
        ::operator delete(ptr);
    }
);

class allocator_guard {
    const allocator* previous_allocator_;
    inline static thread_local const allocator* current_allocator_ = &default_allocator;

  public:
    allocator_guard(const allocator& allocator) : previous_allocator_(current_allocator_) {
        current_allocator_ = &allocator;
    }

    ~allocator_guard() {
        current_allocator_ = previous_allocator_;
    }

    static const allocator* current_allocator() {
        return current_allocator_;
    }
};

inline const allocator& current_allocator() {
    return *allocator_guard::current_allocator();
}

namespace detail {
template<awaitable Awaitable>
class allocator_aware_awaitable {
    const allocator& allocator_;
    Awaitable awaitable_;

  public:
    allocator_aware_awaitable(Awaitable awaitable)
        : allocator_(current_allocator()), awaitable_(std::move(awaitable)) {}

    auto poll(const waker& w) {
        auto guard = allocator_guard(allocator_);
        return awaitable_.poll(w);
    }
};

template<stream_awaitable StreamAwaitable>
class allocator_aware_stream_awaitable {
    const allocator& allocator_;
    StreamAwaitable stream_awaitable_;

  public:
    allocator_aware_stream_awaitable(StreamAwaitable stream_awaitable)
        : allocator_(current_allocator()), stream_awaitable_(std::move(stream_awaitable)) {}

    auto poll_next(const waker& w) {
        auto guard = allocator_guard(allocator_);
        return stream_awaitable_.poll_next(w);
    }
};
}  // namespace detail

template<typename Func, typename... Args>
auto allocator::in_scope(Func&& func, Args&&... args) const {
    auto guard = allocator_guard(*this);
    using result_type =
        decltype(std::invoke(std::forward<Func>(func), std::forward<Args>(args)...));

    if constexpr (awaitable<result_type>) {
        return detail::allocator_aware_awaitable<result_type>(
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...)
        );
    } else if constexpr (stream_awaitable<result_type>) {
        return detail::allocator_aware_stream_awaitable<result_type>(
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...)
        );
    } else {
        return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
    }
}

template<typename Func, typename... Args>
auto allocate_in(const allocator& alloc, Func&& func, Args&&... args) {
    return alloc.in_scope(std::forward<Func>(func), std::forward<Args>(args)...);
}
}  // namespace pollcoro
