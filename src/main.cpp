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
    loop.set_scheduler(scheduler);

    Task hi{10, "HiTask", [](const Event& e) {
        (void)e; // Unused parameter
        for (volatile int i = 0; i < 1000; ++i); // Simulate work
    }};
    Task lo{1, "LoTask", [](const Event& e) {
        (void)e; // Unused parameter
        for (volatile int i = 0; i < 1000; ++i); // Simulate work
    }};
    Task lo_2{2, "AnotherLoTask", [](const Event& e) {
        (void)e; // Unused parameter
        for (volatile int i = 0; i < 500; ++i); // Simulate work
    }};
    Task lo_3{3, "YetAnotherLoTask", [](const Event& e) {
        (void)e; // Unused parameter
        for (volatile int i = 0; i < 500; ++i); // Simulate work
    }};

    // timer to enqueue low priority task every 5ms
    loop.add_timerfd(5, EventType::TimerTick);
    loop.register_task(EventType::TimerTick, lo);
    loop.register_task(EventType::TimerTick, lo_2);
    loop.register_task(EventType::TimerTick, lo_3);

    // event to enqueue high priority task when signaled
    int irq_fd = loop.add_eventfd(EventType::Interrupt);
    loop.register_task(EventType::Interrupt, hi);

    std::atomic<bool> running{true};
    metrics.start_timing();

    // Worker thread to pick and execute tasks
    std::thread worker([&]() {
        while (running.load()) {
            auto item = scheduler.pick_next();
            metrics.record_queue_depth(scheduler.size());
            if (!item) {
                std::this_thread::sleep_for(std::chrono::microseconds(200)); // Sleep briefly if no tasks
                continue;
            }
            uint64_t dispatch_ns = now_ns();
            metrics.record_latency_ns(dispatch_ns - item->event.ts_ns);
            item->task.handler(item->event);
        }
    });

    // Simulate external interrupts every 3ms
    std::thread irq_simulator([&]() {
        while (running.load()) {
            loop.signal_eventfd(irq_fd, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
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