#include <coroutine>
#include <iostream>

import pollcoro;

pollcoro::task<int> async_add(int a, int b) {
    co_return a + b;
}

int main() {
    auto task = async_add(10, 20);
    auto reference = pollcoro::ref(task);
    auto result = pollcoro::block_on(reference);
    std::cout << "Result: " << result << std::endl;
    return 0;
}
