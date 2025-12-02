#pragma once

#ifndef POLLCORO_MODULE_EXPORT
#include <optional>
#endif

#include "export.hpp"

POLLCORO_EXPORT namespace pollcoro {

template<typename T = void>
class pollable_state {
    constexpr explicit pollable_state(T result) : result_(std::move(result)) {}

  public:
    using result_type = T;
    pollable_state() = default;

    static constexpr pollable_state ready(T result) {
        return pollable_state(std::move(result));
    }

    static constexpr pollable_state pending() {
        return pollable_state();
    }

    bool is_ready() const {
        return result_.has_value();
    }

    T take_result() {
        return std::move(*result_);
    }

    template<typename Func>
    auto map(Func&& func) && {
        using U = std::invoke_result_t<Func, T>;
        if (!is_ready()) {
            return pollable_state<U>::pending();
        }
        return pollable_state<U>::ready(func(take_result()));
    }

  private:
    std::optional<T> result_ = std::nullopt;
};

template<>
class pollable_state<void> {
    constexpr pollable_state(bool ready) : ready_(ready) {}

  public:
    using result_type = void;

    pollable_state() = default;

    static constexpr pollable_state ready() {
        return pollable_state(true);
    }

    static constexpr pollable_state pending() {
        return pollable_state(false);
    }

    bool is_ready() const {
        return ready_;
    }

    void take_result() {}

    template<typename Func>
    auto map(Func&& func) && {
        using U = std::invoke_result_t<Func>;
        if (!is_ready()) {
            return pollable_state<U>::pending();
        }
        return pollable_state<U>::ready(func());
    }

  private:
    bool ready_{false};
};

}  // namespace pollcoro
