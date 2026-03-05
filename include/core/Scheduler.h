#pragma once
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <chrono>
#include <core/Task.h>

struct ScheduledItem {
    Task task;
    Event event;
    uint64_t enqueue_ts_ns; // Timestamp when the task was enqueued
    int effective_priority; // Effective priority considering aging
};

class Scheduler {
public:
    // maximum allowed priority - tasks or aging won't raise priority above this
    static constexpr int kMaxPriority = 32;

    // globally control debug logging (default off)
    static bool debug;

    Scheduler();

    void enqueue(const Task& task, const Event& event);
    std::optional<ScheduledItem> pick_next();
    size_t size() const;

private:
    void apply_aging_locked(uint64_t now_ns);
    mutable std::mutex mu_;
    static constexpr uint64_t aging_threshold_ns = 200'000; // 0.2ms
    std::vector<std::deque<ScheduledItem>> queues_; // index 0 to kMaxPriority
    int highest_priority_{-1}; // cache highest non-empty priority level
};