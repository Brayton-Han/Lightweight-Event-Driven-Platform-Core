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
    Scheduler();
    void enqueue(const Task& task, const Event& event);
    std::optional<ScheduledItem> pick_next();
    size_t size() const;

private:
    void apply_aging_locked(uint64_t now_ns);
    mutable std::mutex mu_;
    std::map<int, std::deque<ScheduledItem>, std::greater<int>> priority_queues_; // Map of priority to queue of scheduled items
};