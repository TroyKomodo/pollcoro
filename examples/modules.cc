#include <coroutine>
#include <iostream>

import pollcoro;

pollcoro::task<int> async_add(int a, int b) {
    co_return a + b;
}

int main() {
    int result = pollcoro::block_on(async_add(10, 20));
    std::cout << "Result: " << result << std::endl;
    return 0;
}
