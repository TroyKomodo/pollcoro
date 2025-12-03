/*
 * A example using C++17 coroutines
 */

#include <iostream>
#include <pollcoro/block_on.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/wait_all.hpp>
#include <pollcoro/yield.hpp>

pollcoro::task<> do_work() {
    std::cout << "Doing work..." << std::endl;
    co_await pollcoro::yield();
    std::cout << "Doing more work..." << std::endl;
    co_await pollcoro::yield();
    std::cout << "Doing even more work..." << std::endl;
    co_return;
}

int main() {
    pollcoro::block_on(do_work());
    return 0;
}
