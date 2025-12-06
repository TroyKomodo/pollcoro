#undef linux

#include <chrono>
#include <cppcoro/async_auto_reset_event.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <iostream>
#include <pollcoro/block_on.hpp>
#include <pollcoro/make_pollable.hpp>
#include <pollcoro/make_resumable.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/yield.hpp>
#include <thread>

namespace {
// Utility to print with thread id
template<typename... Args>
void log(Args&&... args) {
    std::cout << "[thread " << std::this_thread::get_id() << "] ";
    (std::cout << ... << std::forward<Args>(args)) << std::endl;
}

// RAII thread that joins on destruction
class scoped_thread {
    std::thread thread_;

  public:
    template<typename F>
    explicit scoped_thread(F&& f) : thread_(std::forward<F>(f)) {}

    ~scoped_thread() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    scoped_thread(scoped_thread&&) = default;
    scoped_thread& operator=(scoped_thread&&) = default;
    scoped_thread(const scoped_thread&) = delete;
    scoped_thread& operator=(const scoped_thread&) = delete;
};
}  // namespace

// -----------------------------------------------------------------------------
// cppcoro primitives
// -----------------------------------------------------------------------------

// Async sleep using cppcoro's event mechanism
cppcoro::task<> async_sleep(std::chrono::milliseconds duration) {
    log("async_sleep: starting ", duration.count(), "ms sleep");

    auto event = std::make_shared<cppcoro::async_auto_reset_event>();

    // Spawn thread to signal after duration
    // Using shared_ptr to ensure event outlives the thread
    std::thread([event, duration] {
        std::this_thread::sleep_for(duration);
        log("async_sleep: timer fired");
        event->set();
    }).detach();

    co_await *event;
    log("async_sleep: resumed");
}

// Async operation that returns a value
cppcoro::task<int> async_compute(int x) {
    log("async_compute: computing with x=", x);
    co_await async_sleep(std::chrono::milliseconds(100));
    co_return x * 2;
}

// -----------------------------------------------------------------------------
// pollcoro primitives
// -----------------------------------------------------------------------------

// Simple pollcoro task demonstrating yield
pollcoro::task<> poll_with_yields(int count) {
    log("poll_with_yields: starting with count=", count);

    for (int i = 0; i < count; ++i) {
        log("poll_with_yields: iteration ", i + 1, "/", count);
        co_await pollcoro::yield();
    }

    log("poll_with_yields: complete");
}

// pollcoro task that uses a cppcoro awaitable via make_pollable
pollcoro::task<int> poll_with_cppcoro_sleep(int value) {
    log("poll_with_cppcoro_sleep: starting");

    co_await pollcoro::make_pollable(async_sleep(std::chrono::milliseconds(500)));

    log("poll_with_cppcoro_sleep: after sleep, returning ", value);
    co_return value;
}

// Chained pollcoro operations
pollcoro::task<int> poll_chain() {
    log("poll_chain: starting");

    auto a = co_await poll_with_cppcoro_sleep(10);
    log("poll_chain: got a=", a);

    auto b = co_await poll_with_cppcoro_sleep(20);
    log("poll_chain: got b=", b);

    co_return a + b;
}

// -----------------------------------------------------------------------------
// Interop demonstrations
// -----------------------------------------------------------------------------

// cppcoro task that drives a pollcoro task via make_resumable
cppcoro::task<int> cppcoro_drives_pollcoro(cppcoro::io_service& io_service) {
    log("cppcoro_drives_pollcoro: starting");

    cppcoro::io_work_scope work_scope(io_service);

    auto result = co_await pollcoro::make_resumable(poll_chain(), io_service);

    log("cppcoro_drives_pollcoro: got result=", result);
    co_return result;
}

// Process io_service events
cppcoro::task<> run_io_service(cppcoro::io_service& io_service) {
    log("run_io_service: processing events");
    io_service.process_events();
    log("run_io_service: done");
    co_return;
}

// -----------------------------------------------------------------------------
// Example runners
// -----------------------------------------------------------------------------

void example_basic_pollcoro() {
    std::cout << "\n=== Example 1: Basic pollcoro with block_on ===\n" << std::endl;

    pollcoro::block_on(poll_with_yields(3));

    std::cout << "\n--- Example 1 complete ---\n";
}

void example_pollcoro_with_cppcoro_awaitable() {
    std::cout << "\n=== Example 2: pollcoro using cppcoro awaitable ===\n" << std::endl;

    auto result = pollcoro::block_on(poll_with_cppcoro_sleep(42));
    log("Result: ", result);

    std::cout << "\n--- Example 2 complete ---\n";
}

void example_cppcoro_with_pollcoro_task() {
    std::cout << "\n=== Example 3: cppcoro driving pollcoro task ===\n" << std::endl;

    cppcoro::io_service io_service;

    auto result = cppcoro::sync_wait(
        cppcoro::when_all(cppcoro_drives_pollcoro(io_service), run_io_service(io_service))
    );

    log("Result: ", std::get<0>(result));

    std::cout << "\n--- Example 3 complete ---\n";
}

void example_error_handling() {
    std::cout << "\n=== Example 4: Error handling ===\n" << std::endl;

    auto failing_task = []() -> pollcoro::task<int> {
        log("failing_task: about to throw");
        co_await pollcoro::yield();
        throw std::runtime_error("intentional failure");
        co_return 0;  // unreachable
    };

    try {
        pollcoro::block_on(failing_task());
        log("ERROR: should not reach here");
    } catch (const std::runtime_error& e) {
        log("Caught expected exception: ", e.what());
    }

    std::cout << "\n--- Example 4 complete ---\n";
}

void example_concurrent_tasks() {
    std::cout << "\n=== Example 5: Concurrent cppcoro tasks with pollcoro ===\n" << std::endl;

    cppcoro::io_service io_service;

    auto task_a = [&]() -> cppcoro::task<int> {
        cppcoro::io_work_scope work_scope(io_service);
        log("task_a: starting");
        co_await pollcoro::make_resumable(poll_with_cppcoro_sleep(100), io_service);
        log("task_a: done");
        co_return 1;
    };

    auto task_b = [&]() -> cppcoro::task<int> {
        cppcoro::io_work_scope work_scope(io_service);
        log("task_b: starting");
        co_await pollcoro::make_resumable(poll_with_cppcoro_sleep(200), io_service);
        log("task_b: done");
        co_return 2;
    };

    auto [a, b, _] =
        cppcoro::sync_wait(cppcoro::when_all(task_a(), task_b(), run_io_service(io_service)));

    log("Results: a=", a, ", b=", b);

    std::cout << "\n--- Example 5 complete ---\n";
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    std::cout << "pollcoro + cppcoro interop demonstration\n";
    std::cout << "Main thread: " << std::this_thread::get_id() << "\n";
    std::cout << std::string(50, '=') << std::endl;

    try {
        example_basic_pollcoro();
        example_pollcoro_with_cppcoro_awaitable();
        example_cppcoro_with_pollcoro_task();
        example_error_handling();
        example_concurrent_tasks();

        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "All examples completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
