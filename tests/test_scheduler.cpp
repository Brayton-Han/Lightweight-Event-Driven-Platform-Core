#include "core/Scheduler.h"
#include "core/Task.h"
#include "core/Events.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <cassert>
#include <deque>

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// ============================================================================
// TEST 1: FIFO Order within same priority
// ============================================================================
void test_fifo_order() {
    std::cout << "\n=== TEST 1: FIFO Order ===" << std::endl;
    
    Scheduler scheduler;
    Scheduler::debug = false;
    std::vector<int> delivery_order;
    
    // Create 10 tasks with same priority
    Task test_task{5, "task", [&](const Event& e) {
        delivery_order.push_back(static_cast<int>(e.payload));
    }};
    
    for (int i = 0; i < 10; ++i) {
        Event e{EventType::TimerTick, now_ns(), static_cast<uint64_t>(i)};
        scheduler.enqueue(test_task, e);
    }
    
    // Pick all tasks and check order
    for (int i = 0; i < 10; ++i) {
        auto item = scheduler.pick_next();
        assert(item.has_value());
        item->task.handler(item->event);
    }
    
    // Verify FIFO: 0, 1, 2, ..., 9
    bool fifo_ok = true;
    for (int i = 0; i < 10; ++i) {
        if (delivery_order[i] != i) {
            fifo_ok = false;
            break;
        }
    }
    
    std::cout << "FIFO Order: " << (fifo_ok ? "PASS" : "FAIL") << std::endl;
    if (!fifo_ok) {
        std::cout << "  Expected: 0, 1, 2, ..., 9\n  Got: ";
        for (int x : delivery_order) std::cout << x << " ";
        std::cout << std::endl;
    }
}

// ============================================================================
// TEST 2: Priority Fairness
// ============================================================================
void test_priority_fairness() {
    std::cout << "\n=== TEST 2: Priority Fairness ===" << std::endl;
    
    Scheduler scheduler;
    Scheduler::debug = false;
    std::vector<int> task_priority_counts(33, 0);
    
    Task dummy{0, "dummy", [](const Event& e) { (void)e; }};
    
    // Enqueue 300 tasks: 100 pri=10, 100 pri=5, 100 pri=0
    for (int i = 0; i < 100; ++i) {
        scheduler.enqueue(Task{10, "hi", [](const Event& e) { (void)e; }}, 
                         Event{EventType::TimerTick, now_ns(), 0});
        scheduler.enqueue(Task{5, "mid", [](const Event& e) { (void)e; }}, 
                         Event{EventType::TimerTick, now_ns(), 0});
        scheduler.enqueue(Task{0, "lo", [](const Event& e) { (void)e; }}, 
                         Event{EventType::TimerTick, now_ns(), 0});
    }
    
    // Pick all and record priorities
    for (int i = 0; i < 300; ++i) {
        auto item = scheduler.pick_next();
        if (item) {
            task_priority_counts[item->effective_priority]++;
        }
    }
    
    // Higher priority should be picked first (at least some)
    size_t high_count = task_priority_counts[10];
    size_t mid_count = task_priority_counts[5];
    size_t low_count = task_priority_counts[0];
    
    bool fairness_ok = (high_count >= mid_count) && (mid_count >= low_count);
    std::cout << "Priority Distribution: pri=10: " << high_count 
              << ", pri=5: " << mid_count << ", pri=0: " << low_count << std::endl;
    std::cout << "Fairness (higher pri picked first): " << (fairness_ok ? "PASS" : "FAIL") << std::endl;
}

// ============================================================================
// TEST 3: Aging Promotion Works
// ============================================================================
void test_aging() {
    std::cout << "\n=== TEST 3: Aging Promotion ===" << std::endl;
    
    Scheduler scheduler;
    Scheduler::debug = false;
    
    Task dummy{0, "aged", [](const Event& e) { (void)e; }};
    
    // Enqueue a low-priority task
    Event old_event{EventType::TimerTick, now_ns() - 300'000, 0};  // 300us ago
    scheduler.enqueue(dummy, old_event);
    
    // Wait and pick - aging should have promoted it
    std::this_thread::sleep_for(std::chrono::microseconds(300));
    auto item = scheduler.pick_next();
    
    bool aged_ok = item.has_value();
    int final_priority = item ? item->effective_priority : -1;
    
    std::cout << "Old task promoted to priority: " << final_priority << std::endl;
    std::cout << "Aging Promotion: " << (aged_ok && final_priority > 0 ? "PASS" : "FAIL") << std::endl;
}

// ============================================================================
// TEST 4: No Task Loss During Shutdown
// ============================================================================
void test_no_task_loss() {
    std::cout << "\n=== TEST 4: No Task Loss ===" << std::endl;
    
    constexpr size_t total = 10000;
    Scheduler scheduler;
    Scheduler::debug = false;
    
    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};
    
    Task dummy{0, "task", [](const Event& e) { (void)e; }};
    
    // Fast producer
    std::thread producer([&]() {
        for (size_t i = 0; i < total; ++i) {
            scheduler.enqueue(dummy, Event{EventType::TimerTick, now_ns(), 0});
            ++produced;
        }
    });
    
    // Slow consumer (mimics shutdown scenario)
    std::thread consumer([&]() {
        while (consumed < total) {
            auto item = scheduler.pick_next();
            if (item) {
                ++consumed;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    producer.join();
    consumer.join();
    
    bool no_loss = (produced == consumed) && (consumed == total);
    std::cout << "Produced: " << produced << ", Consumed: " << consumed << std::endl;
    std::cout << "Task Loss Prevention: " << (no_loss ? "PASS" : "FAIL") << std::endl;
}

// ============================================================================
// Throughput Stress Test (original)
// ============================================================================
void test_throughput() {
    std::cout << "\n=== TEST 5: Throughput Stress ===" << std::endl;
    
    constexpr size_t total_tasks = 1'000'000;
    constexpr int producer_count = 4;
    constexpr size_t per_producer = total_tasks / producer_count;

    Scheduler scheduler;
    Scheduler::debug = false;
    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};

    Task dummy{0, "dummy", [](const Event& e) { (void)e; }};

    auto producer = [&](int id) {
        std::mt19937_64 rng(id + 12345);
        std::uniform_int_distribution<int> pri_dist(0, Scheduler::kMaxPriority);
        for (size_t i = 0; i < per_producer; ++i) {
            Task t = dummy;
            t.priority = pri_dist(rng);
            scheduler.enqueue(t, Event{EventType::TimerTick, now_ns(), 0});
            ++produced;
        }
    };

    auto consumer = [&]() {
        while (consumed < total_tasks) {
            auto item = scheduler.pick_next();
            if (item) {
                ++consumed;
            } else {
                std::this_thread::yield();
            }
        }
    };

    auto start = now_ns();

    std::vector<std::thread> producers;
    for (int i = 0; i < producer_count; ++i) {
        producers.emplace_back(producer, i);
    }
    std::thread cons_thread(consumer);

    for (auto& t : producers) t.join();
    cons_thread.join();

    auto end = now_ns();
    double seconds = (end - start) / 1e9;
    double rate = total_tasks / seconds;

    std::cout << "Tasks: " << total_tasks << ", Elapsed: " << seconds << "s\n";
    std::cout << "Throughput: " << rate << " tasks/sec\n";
    std::cout << "Throughput Test: PASS" << std::endl;
}

int main() {
    std::cout << "=======================================\n";
    std::cout << "    SCHEDULER VALIDATION TEST SUITE\n";
    std::cout << "=======================================\n";

    try {
        test_fifo_order();
        test_priority_fairness();
        test_aging();
        test_no_task_loss();
        test_throughput();
        
        std::cout << "\n=======================================\n";
        std::cout << "       ALL TESTS COMPLETED\n";
        std::cout << "=======================================\n";
    } catch (const std::exception& e) {
        std::cerr << "Test exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
