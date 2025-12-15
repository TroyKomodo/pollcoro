#include <coroutine>
#include <iostream>

import pollcoro;

pollcoro::task<int> async_add(int a, int b) {
    co_return a + b;
}

void test_reference() {
    auto task = async_add(10, 20);
    auto reference = pollcoro::ref(task);
    auto result = pollcoro::block_on(reference);
    std::cout << "Result: " << result << std::endl;
}

void test_reference_stream() {
    auto stream = pollcoro::range(10);
    auto reference = pollcoro::ref(stream);
    while (auto value = pollcoro::block_on(pollcoro::next(reference))) {
        std::cout << "Result: " << *value << std::endl;
    }
}

void test_reference_rvalue() {
    auto result = pollcoro::block_on(pollcoro::ref(async_add(10, 20)));
    std::cout << "Result: " << result << std::endl;
}

int main() {
    test_reference();
    test_reference_stream();
    test_reference_rvalue();
    return 0;
}
