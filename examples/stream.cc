#include <iostream>
#include <pollcoro/block_on.hpp>
#include <pollcoro/chain.hpp>
#include <pollcoro/enumerate.hpp>
#include <pollcoro/flatten.hpp>
#include <pollcoro/fold.hpp>
#include <pollcoro/iter.hpp>
#include <pollcoro/last.hpp>
#include <pollcoro/map.hpp>
#include <pollcoro/next.hpp>
#include <pollcoro/nth.hpp>
#include <pollcoro/skip.hpp>
#include <pollcoro/skip_while.hpp>
#include <pollcoro/stream.hpp>
#include <pollcoro/sync_iter.hpp>
#include <pollcoro/take.hpp>
#include <pollcoro/take_while.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/window.hpp>
#include <pollcoro/yield.hpp>
#include <pollcoro/zip.hpp>

// Simple synchronous stream
pollcoro::stream<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield b;
        auto tmp = a;
        a = b;
        b += tmp;
    }
}

// Stream that uses co_await internally (async stream)
pollcoro::stream<int> async_counter(int count) {
    for (int i = 0; i < count; ++i) {
        // Simulate async work with yield
        co_await pollcoro::yield();
        co_yield i;
    }
}

// Helper stream for yield-from demo
pollcoro::stream<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

pollcoro::stream<int> repeat(int value) {
    while (true) {
        co_yield value;
    }
}

// Stream that yields from other streams
pollcoro::stream<int> combined_ranges() {
    co_yield 100;  // Single value
    co_yield range(0, 3);  // Yield from: 0, 1, 2
    co_yield 200;  // Single value
    co_yield range(10, 13);  // Yield from: 10, 11, 12
    co_yield 300;  // Single value
    co_yield pollcoro::iter(std::array{13, 14, 15});  // From iterator
}

pollcoro::task<> test_fibonacci() {
    std::cout << "Fibonacci sequence (first 10 over 100):" << std::endl;
    auto skip_while_pred = [](auto n) {
        return n < 100;
    };

    auto take_while_pred = [](auto n) {
        return n > 0;
    };

    auto s = fibonacci() | pollcoro::skip_while(skip_while_pred) | pollcoro::take(10);

    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }

    std::cout << std::endl;
}

pollcoro::task<> test_async_counter() {
    std::cout << "Async counter:" << std::endl;
    auto s = async_counter(5);
    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
}

pollcoro::task<> test_yield_from() {
    std::cout << "Yield from (combined ranges):" << std::endl;
    auto s = combined_ranges();
    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
    // Expected: 100 0 1 2 200 10 11 12 300
}

pollcoro::task<> test_chain() {
    std::cout << "Chain:" << std::endl;
    auto s = fibonacci() | pollcoro::take(3) | pollcoro::chain(async_counter(5)) |
        pollcoro::map([](auto n) {
                 return n * 2;
             });
    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
}

pollcoro::task<> test_flatten() {
    auto map_op = [](auto n) {
        return repeat(n) | pollcoro::take(n);
    };

    auto s = async_counter(5) | pollcoro::map(map_op) | pollcoro::flatten();

    std::cout << "Flatten:" << std::endl;
    while (auto value = co_await pollcoro::next(s)) {
        std::cout << *value << " ";
    }
    std::cout << std::endl;
}

pollcoro::task<> test_zip() {
    std::cout << "Zip:" << std::endl;
    auto s = zip(async_counter(5), async_counter(5), pollcoro::enumerate()) | pollcoro::take(3);
    while (auto value = co_await pollcoro::next(s)) {
        auto [a, b, index] = *value;
        std::cout << index << ": (" << a << ", " << b << ") ";
    }
    std::cout << std::endl;
}

pollcoro::task<> test_last() {
    std::cout << "Last:" << std::endl;
    auto s = co_await pollcoro::last(async_counter(5));
    std::cout << *s << std::endl;
}

pollcoro::task<> test_nth() {
    std::cout << "Nth (3):" << std::endl;
    auto s = async_counter(5);
    auto nth = co_await pollcoro::nth(s, 3);
    std::cout << *nth << std::endl;

    std::cout << "Nth (2):" << std::endl;

    auto s2 = async_counter(5);
    while (auto nth = co_await pollcoro::nth(s2, 2)) {
        std::cout << *nth << " ";
    }
    std::cout << std::endl;
}

pollcoro::task<> test_window() {
    std::cout << "Window (3):" << std::endl;
    auto s = async_counter(10) | pollcoro::window<3>();
    while (auto value = co_await pollcoro::next(s)) {
        auto [a, b, c] = *value;
        std::cout << "(" << a << ", " << b << ", " << c << ") ";
    }
    std::cout << std::endl;
}

pollcoro::task<> test_fold() {
    std::cout << "Fold:" << std::endl;
    auto s = co_await pollcoro::fold(async_counter(10000), 0, [](auto& accumulator, auto n) {
        accumulator += n;
        return accumulator < 50;
    });
    std::cout << s << std::endl;
}

void test_sync_iter() {
    std::cout << "Sync iter:" << std::endl;
    auto s = async_counter(5);
    for (auto value : pollcoro::sync_iter(std::move(s))) {
        std::cout << value << " ";
    }
    std::cout << std::endl;
}

int main() {
    pollcoro::block_on(test_fibonacci());
    pollcoro::block_on(test_async_counter());
    pollcoro::block_on(test_yield_from());
    pollcoro::block_on(test_chain());
    pollcoro::block_on(test_flatten());
    pollcoro::block_on(test_zip());
    pollcoro::block_on(test_last());
    pollcoro::block_on(test_nth());
    pollcoro::block_on(test_window());
    pollcoro::block_on(test_fold());
    test_sync_iter();
    return 0;
}
