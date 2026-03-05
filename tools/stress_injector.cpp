#include "core/EventLoop.h"
#include "core/Scheduler.h"
#include "core/Metrics.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <iomanip>

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void usage(const char* prog) {
    std::cout << "Usage: " << prog << " [--rate EVENTS_PER_SEC] [--duration SECONDS]\n";
    std::cout << "  --rate        Event injection rate in events/sec (default: 50000)\n";
    std::cout << "  --duration    Test duration in seconds (default: 5)\n";
}

int main(int argc, char* argv[]) {
    uint64_t rate = 50000;  // default 50k events/sec
    int duration = 5;       // default 5 seconds

    // parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--rate" && i + 1 < argc) {
            rate = std::atoll(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration = std::atoi(argv[++i]);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    std::cout << "=== EventLoop Stress Injector ===\n";
    std::cout << "Rate: " << rate << " events/sec\n";
    std::cout << "Duration: " << duration << " seconds\n\n";

    Scheduler scheduler;
    Scheduler::debug = false;
    Metrics metrics;
    EventLoop loop;
    loop.set_scheduler(scheduler);

    std::atomic<uint64_t> event_count{0};
    std::atomic<bool> running{true};

    // Simple task handler
    Task simple_task{5, "EventTask", [](const Event& e) {
        (void)e;
        // minimal work
        for (volatile int i = 0; i < 100; ++i);
    }};

    // Register task for TimerTick events
    loop.register_task(EventType::TimerTick, simple_task);

    // Event injector thread - injects events at target rate
    std::thread injector([&]() {
        uint64_t interval_ns = 1'000'000'000ULL / rate;  // nanoseconds between events
        uint64_t next_event_ns = now_ns() + interval_ns;
        
        while (running.load()) {
            uint64_t current_ns = now_ns();
            if (current_ns >= next_event_ns) {
                Event e{EventType::TimerTick, current_ns, 0};
                scheduler.enqueue(simple_task, e);
                ++event_count;
                next_event_ns += interval_ns;
            } else {
                // busy-wait with minimal overhead
                std::this_thread::yield();
            }
        }
    });

    // Worker thread - picks and executes tasks
    std::thread worker([&]() {
        while (running.load()) {
            auto item = scheduler.pick_next();
            if (item) {
                metrics.record_queue_depth(scheduler.size());
                uint64_t dispatch_ns = now_ns();
                metrics.record_latency_ns(dispatch_ns - item->event.ts_ns);
                item->task.handler(item->event);
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    });

    // Stopper thread - stops after duration
    std::thread stopper([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(duration));
        running.store(false);
    });

    metrics.start_timing();
    stopper.join();
    injector.join();
    worker.join();

    std::cout << "Total events injected: " << event_count << "\n";
    std::cout << "Scheduler queue size at end: " << scheduler.size() << "\n";
    metrics.report();

    return 0;
}
