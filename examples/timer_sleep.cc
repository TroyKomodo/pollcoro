#include <coroutine>
#include <functional>
#include <iostream>
#include <thread>

import pollcoro;

template<typename Clock = std::chrono::steady_clock>
struct my_timer {
    using duration = typename Clock::duration;
    using time_point = typename Clock::time_point;

    time_point now() const {
        return Clock::now();
    }

    void register_callback(const time_point& deadline, std::function<void()> callback) {
        std::thread([deadline, callback] {
            std::this_thread::sleep_until(deadline);
            callback();
        }).detach();
    }
};

pollcoro::task<> do_work() {
    std::cout << "Sleeping for 10 seconds" << std::endl;
    co_await pollcoro::sleep_until<my_timer<>>(
        std::chrono::steady_clock::now() + std::chrono::seconds(10)
    );
    std::cout << "Done sleeping" << std::endl;
    co_return;
}

int main() {
    pollcoro::block_on(do_work());
    return 0;
}
