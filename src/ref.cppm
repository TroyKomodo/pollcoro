module;

#include <type_traits>
#include <utility>

export module pollcoro:ref;

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
namespace detail {
template<typename T, typename = void>
struct awaitable_access;

// Direct awaitable
template<typename T>
requires awaitable<T>
struct awaitable_access<T> {
    using type = T&;

    static constexpr type get(T& t) {
        return t;
    }
};

// Dereferenceable to awaitable (shared_ptr, unique_ptr, etc.)
template<typename T>
requires(!awaitable<T>) && requires(T t) { *t; } &&
    awaitable<std::remove_cvref_t<decltype(*std::declval<T&>())>>
struct awaitable_access<T> {
    using type = decltype(*std::declval<T&>());

    static constexpr type get(T& t) {
        return *t;
    }
};

// Has .get() (reference_wrapper, etc.)
template<typename T>
requires(!awaitable<T>) &&
    (!requires(T t) { *t; } || !awaitable<std::remove_cvref_t<decltype(*std::declval<T&>())>>) &&
    requires(T t) { t.get(); } && awaitable<std::remove_cvref_t<decltype(std::declval<T&>().get())>>
struct awaitable_access<T> {
    using type = decltype(std::declval<T&>().get());

    static constexpr type get(T& t) {
        return t.get();
    }
};

template<typename T>
using awaitable_access_t = std::remove_cvref_t<typename awaitable_access<T>::type>;

template<typename T, typename = void>
struct stream_awaitable_access;

// Direct awaitable
template<typename T>
requires stream_awaitable<T>
struct stream_awaitable_access<T> {
    using type = T&;

    static constexpr type get(T& t) {
        return t;
    }
};

// Dereferenceable to awaitable (shared_ptr, unique_ptr, etc.)
template<typename T>
requires(!stream_awaitable<T>) && requires(T t) { *t; } &&
    stream_awaitable<std::remove_cvref_t<decltype(*std::declval<T&>())>>
struct stream_awaitable_access<T> {
    using type = decltype(*std::declval<T&>());

    static constexpr type get(T& t) {
        return *t;
    }
};

// Has .get() (reference_wrapper, etc.)
template<typename T>
requires(!stream_awaitable<T>) && (!requires(T t) {
            *t;
        } || !stream_awaitable<std::remove_cvref_t<decltype(*std::declval<T&>())>>) &&
    requires(T t) {
        t.get();
    } && stream_awaitable<std::remove_cvref_t<decltype(std::declval<T&>().get())>>
struct stream_awaitable_access<T> {
    using type = decltype(std::declval<T&>().get());

    static constexpr type get(T& t) {
        return t.get();
    }
};

template<typename T>
using stream_awaitable_access_t = std::remove_cvref_t<typename stream_awaitable_access<T>::type>;
}  // namespace detail

template<typename AwaitableRef>
class ref_awaitable : public awaitable_maybe_blocks<detail::awaitable_access_t<AwaitableRef>> {
    AwaitableRef awaitable_;

  public:
    ref_awaitable(AwaitableRef awaitable) : awaitable_(std::forward<AwaitableRef>(awaitable)) {}

    auto poll(const waker& w) {
        return detail::awaitable_access<AwaitableRef>::get(awaitable_).poll(w);
    }
};

template<typename StreamAwaitableRef>
class ref_stream_awaitable
    : public awaitable_maybe_blocks<detail::stream_awaitable_access_t<StreamAwaitableRef>> {
    StreamAwaitableRef stream_;

  public:
    ref_stream_awaitable(StreamAwaitableRef stream)
        : stream_(std::forward<StreamAwaitableRef>(stream)) {}

    auto poll_next(const waker& w) {
        return detail::stream_awaitable_access<StreamAwaitableRef>::get(stream_).poll_next(w);
    }
};

template<typename AwaitableRef>
requires awaitable<detail::awaitable_access_t<AwaitableRef>>
auto ref(AwaitableRef&& awaitable) {
    return ref_awaitable<AwaitableRef>(std::forward<AwaitableRef>(awaitable));
}

template<typename StreamAwaitableRef>
requires stream_awaitable<detail::stream_awaitable_access_t<StreamAwaitableRef>>
auto ref(StreamAwaitableRef&& stream) {
    return ref_stream_awaitable<StreamAwaitableRef>(std::forward<StreamAwaitableRef>(stream));
}
}  // namespace pollcoro
