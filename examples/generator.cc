#include <iostream>
#include <pollcoro/block_on.hpp>
#include <pollcoro/gen.hpp>
#include <pollcoro/gen_next.hpp>
#include <pollcoro/generator.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/yield.hpp>

// Simple synchronous generator
pollcoro::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield b;
        auto tmp = a;
        a = b;
        b += tmp;
    }
}

// Generator that uses co_await internally (async generator)
pollcoro::generator<int> async_counter(int count) {
    for (int i = 0; i < count; ++i) {
        // Simulate async work with yield
        co_await pollcoro::yield();
        co_yield i;
    }
}

// Helper generator for yield-from demo
pollcoro::generator<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

// Generator that yields from other generators
pollcoro::generator<int> combined_ranges() {
    co_yield 100;  // Single value
    co_yield range(0, 3);  // Yield from: 0, 1, 2
    co_yield 200;  // Single value
    co_yield range(10, 13);  // Yield from: 10, 11, 12
    co_yield 300;  // Single value
    co_yield pollcoro::gen(std::array{13, 14, 15});  // From iterator
}

pollcoro::task<> test_fibonacci() {
    std::cout << "Fibonacci sequence (first 10 over 100):" << std::endl;
    auto gen = fibonacci();
    int count = 0;
    while (auto value = co_await pollcoro::gen_next(gen)) {
        if (*value > 100) {
            std::cout << *value << " ";
            if (++count >= 10)
                break;
        }
    }
    std::cout << std::endl;
}

pollcoro::task<> test_async_counter() {
    std::cout << "Async counter:" << std::endl;
    auto gen = async_counter(5);
    while (auto value = co_await pollcoro::gen_next(gen)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
}

pollcoro::task<> test_yield_from() {
    std::cout << "Yield from (combined ranges):" << std::endl;
    auto gen = combined_ranges();
    while (auto value = co_await pollcoro::gen_next(gen)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
    // Expected: 100 0 1 2 200 10 11 12 300
}

int main() {
    pollcoro::block_on(test_fibonacci());
    pollcoro::block_on(test_async_counter());
    pollcoro::block_on(test_yield_from());
    return 0;
}
