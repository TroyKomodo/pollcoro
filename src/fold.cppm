module;

#if !defined(POLLCORO_IMPORT_STD) || POLLCORO_IMPORT_STD == 0
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>
#endif

export module pollcoro:fold;

#if defined(POLLCORO_IMPORT_STD) && POLLCORO_IMPORT_STD == 1
import std;
#endif

import :awaitable;
import :is_blocking;
import :stream_awaitable;
import :waker;

export namespace pollcoro {
namespace detail {
template<typename T>
concept bool_or_void = std::same_as<T, bool> || std::same_as<T, void>;

template<typename FoldFunction, typename Accumulator, typename ResultType>
concept stream_fold_function =
    requires(FoldFunction fold_function, Accumulator& accumulator, ResultType result) {
        { fold_function(accumulator, result) } -> bool_or_void;
    };
}  // namespace detail

template<
    stream_awaitable StreamAwaitable,
    typename Accumulator,
    detail::stream_fold_function<Accumulator, stream_awaitable_result_t<StreamAwaitable>>
        FoldFunction>
class fold_stream_awaitable : public awaitable_maybe_blocks<StreamAwaitable> {
    StreamAwaitable stream_;
    Accumulator accumulator_;
    FoldFunction fold_function_;

    using result_type = Accumulator;
    using state_type = awaitable_state<result_type>;
    using fold_return_type = std::
        invoke_result_t<FoldFunction, Accumulator&, stream_awaitable_result_t<StreamAwaitable>>;

  public:
    fold_stream_awaitable(
        StreamAwaitable&& stream, Accumulator&& accumulator, FoldFunction&& fold_function
    )
        : stream_(std::move(stream)),
          accumulator_(std::move(accumulator)),
          fold_function_(std::move(fold_function)) {}

    state_type poll(const waker& w) {
        auto state = stream_.poll_next(w);
        if (state.is_done()) {
            return state_type::ready(std::move(accumulator_));
        }
        if (state.is_ready()) {
            if constexpr (std::is_same_v<fold_return_type, void>) {
                std::invoke(fold_function_, accumulator_, state.take_result());
            } else {
                if (!std::invoke(fold_function_, accumulator_, state.take_result())) {
                    return state_type::ready(std::move(accumulator_));
                }
            }
            w.wake();
        }
        return state_type::pending();
    }
};

template<
    stream_awaitable StreamAwaitable,
    typename Accumulator,
    detail::stream_fold_function<Accumulator, stream_awaitable_result_t<StreamAwaitable>>
        FoldFunction>
fold_stream_awaitable(StreamAwaitable, Accumulator, FoldFunction)
    -> fold_stream_awaitable<StreamAwaitable, Accumulator, FoldFunction>;

template<
    stream_awaitable StreamAwaitable,
    typename Accumulator,
    detail::stream_fold_function<
        std::remove_cvref_t<Accumulator>,
        stream_awaitable_result_t<std::remove_cvref_t<StreamAwaitable>>> FoldFunction>
constexpr auto
fold(StreamAwaitable&& stream, Accumulator&& accumulator, FoldFunction&& fold_function) {
    return fold_stream_awaitable(
        std::move(stream), std::move(accumulator), std::move(fold_function)
    );
}
}  // namespace pollcoro
