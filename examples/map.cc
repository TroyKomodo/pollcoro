/*
 * Clang Lambda Move Constructor Bug Reproduction
 *
 * This example demonstrates a potential Clang 20 bug where lambdas with
 * captures fail to have valid move constructors in C++20 module contexts.
 *
 * The issue manifests when:
 * 1. A lambda with captures (e.g., [this] or [&])
 * 2. Is used as a template parameter
 * 3. In a type that needs to be moved
 * 4. Within a C++20 module context
 *
 * Workarounds:
 * - Use +[] for stateless lambdas (converts to function pointer)
 * - Wrap in std::function
 * - Use a named functor class
 */

#include <coroutine>
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

import pollcoro;

struct NonMovable {
    NonMovable() = default;
    NonMovable(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = delete;
    NonMovable& operator=(const NonMovable&) = delete;
    NonMovable& operator=(NonMovable&&) = delete;
};

struct MyClass {
    int value = 0;
    NonMovable non_movable;

    // This lambda captures 'this' - fails with Clang 20 modules bug
    auto test() {
        auto [event, setter] = pollcoro::single_event<int>();
        setter.set(42);
        return pollcoro::map(std::move(event), [this](int x) {
            return ([this](int x) {
                this->value = x;
                return x * 2;
            })(x);
        });
    }
};

pollcoro::task<int> workaround_functor(MyClass& obj) {
    co_return co_await obj.test();
}

int main() {
    MyClass obj;
    pollcoro::block_on(workaround_functor(obj));
    return 0;
}
