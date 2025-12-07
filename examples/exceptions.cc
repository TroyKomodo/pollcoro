#include <coroutine>
#include <iostream>
#include <stdexcept>

import pollcoro;

pollcoro::task<int> async_divide(int a, int b) {
    if (b == 0) {
        throw std::runtime_error("division by zero");
    }

    co_return a / b;
}

pollcoro::task<int> do_work() {
    try {
        auto result = co_await async_divide(10, 0);
        co_return result;
    } catch (const std::runtime_error& e) {
        std::cout << "Caught exception at do_work: " << e.what() << std::endl;
        throw;
    }
}

pollcoro::task<int> catch_exception() {
    try {
        co_return co_await do_work();
    } catch (const std::runtime_error& e) {
        std::cout << "Caught exception at catch_exception: " << e.what() << std::endl;
        co_return 0;
    }
}

int main() {
    try {
        auto result = pollcoro::block_on(catch_exception());
        std::cout << "Result: " << result << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "Caught exception at main: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
