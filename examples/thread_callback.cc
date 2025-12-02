/*
* A example using a thread callback
* Notice how even though we call `setter.set(42)` 
* in a different thread, the task is resumed on 
* the main thread.
*/

#include <pollcoro/task.hpp>
#include <pollcoro/block_on.hpp>
#include <pollcoro/yield.hpp>
#include <pollcoro/wait_all.hpp>
#include <pollcoro/single_event.hpp>
#include <iostream>
#include <thread>

template<typename... Args>
void log(Args&&... args) {
    std::cout << "[" << std::this_thread::get_id() << "] ";
    (std::cout << ... << args) << std::endl;
}


pollcoro::task<> do_work() {
    log("Doing work...");
    auto [awaitable, setter] = pollcoro::single_event<int>();
    std::thread t([setter = std::move(setter)]() mutable {
        log("Sleeping for 1 second...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        setter.set(42);
    });
    t.detach();
    log("Waiting for result...");
    int result = co_await std::move(awaitable);
    log("Result: ", result);
    co_return;
}

int main() {
    log("starting!");
    pollcoro::block_on(do_work());
    return 0;
}