/*
 * Fibonacci using pollcoro
 */

#include <coroutine>
#include <iostream>
#include <optional>
#include <tuple>

import pollcoro;

pollcoro::task<int> async_fibonacci(int n) {
    if (n <= 1) {
        co_return n;
    }

    auto [a, b] = co_await pollcoro::wait_all(async_fibonacci(n - 1), async_fibonacci(n - 2));

    co_return a + b;
}

int main() {
    int result = pollcoro::block_on(async_fibonacci(10));
    std::cout << "Fibonacci(10) = " << result << std::endl;
    return 0;
}
