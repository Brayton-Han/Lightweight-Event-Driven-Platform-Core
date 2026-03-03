#include "core/EventLoop.h"
#include "core/Scheduler.h"
#include "core/Metrics.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int main() {
    std::cout << "Hello, welcome to the Event-Driven Platform!" << std::endl;

    Scheduler scheduler;
    Metrics metrics;
    EventLoop loop;

    Task hi{10, "HiTask", [](const Event& e) {
        (void)e; // Unused parameter
        for (volatile int i = 0; i < 1000; ++i); // Simulate work
    }};
    Task lo{1, "LoTask", [](const Event& e) {
        (void)e; // Unused parameter
        for (volatile int i = 0; i < 1000; ++i); // Simulate work
    }};

    // timer to enqueue low priority task every 10ms
    loop.add_timerfd(10, [&](const Event& e) {
        scheduler.enqueue(lo, e);
    });

    // event to enqueue high priority task when signaled
    int irq_fd = loop.add_eventfd([&](const Event& e) {
        scheduler.enqueue(hi, e);
    });

    std::atomic<bool> running{true};

    // Worker thread to pick and execute tasks
    std::thread worker([&]() {
        while (running.load()) {
            auto item = scheduler.pick_next();
            if (!item) {
                std::this_thread::sleep_for(std::chrono::microseconds(200)); // Sleep briefly if no tasks
                continue;
            }
            uint64_t dispatch_ns = now_ns();
            metrics.record_latency_ns(dispatch_ns - item->event.ts_ns);
            item->task.handler(item->event);
        }
    });

    // Simulate external interrupts every 7ms
    std::thread irq_simulator([&]() {
        while (running.load()) {
            loop.signal_eventfd(irq_fd, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(7));
        }
    });

    // Stop the system after 3 seconds
    std::thread stopper([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        running.store(false);
        loop.stop();
    });

    loop.run();
    stopper.join();
    irq_simulator.join();
    worker.join();

    metrics.report();
    std::cout << "Goodbye!" << std::endl;
    return 0;
}