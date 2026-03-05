#include "core/Scheduler.h"
#include <iostream>

// debug flag definition
bool Scheduler::debug = false;

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

Scheduler::Scheduler() {
    queues_.resize(kMaxPriority + 1);
}

void Scheduler::enqueue(const Task& task, const Event& event) {
    std::lock_guard<std::mutex> lk(mu_);
    int pri = std::min(task.priority, kMaxPriority);
    queues_[pri].push_back({task, event, now_ns(), pri});
    if (pri > highest_priority_) highest_priority_ = pri;
    if (debug) std::cout << "[sched] enqueue '" << task.name << "' pri=" << pri << "\n";
}

std::optional<ScheduledItem> Scheduler::pick_next() {
    std::lock_guard<std::mutex> lk(mu_);
    uint64_t now = now_ns();
    apply_aging_locked(now);

    for (int pri = highest_priority_; pri >= 0; --pri) {
        auto& queue = queues_[pri];
        if (!queue.empty()) {
            ScheduledItem item = std::move(queue.front());
            queue.pop_front();
            if (queue.empty() && pri == highest_priority_) {
                // update cache when highest priority queue becomes empty
                while (highest_priority_ >= 0 && queues_[highest_priority_].empty()) {
                    --highest_priority_;
                }
            }

            auto latency = now - item.event.ts_ns;
            if (debug) std::cout << "[sched] dispatch '" << item.task.name << "' pri="
                      << item.effective_priority << " latency=" << latency << "ns\n";
            return item;
        }
    }
    highest_priority_ = -1;
    return std::nullopt; // No tasks available
}

void Scheduler::apply_aging_locked(uint64_t now) {
    for (int pri = highest_priority_; pri >= 0; --pri) {
        auto& queue = queues_[pri];
        for (auto qit = queue.begin(); qit != queue.end(); /* no increment here */) {
            if (now - qit->enqueue_ts_ns > aging_threshold_ns && qit->effective_priority < kMaxPriority) {
                int new_pri = qit->effective_priority + 1;
                if (debug) std::cout << "[sched] aging promote '" << qit->task.name << "' to pri=" << new_pri << "\n";
                auto item = std::move(*qit);
                item.effective_priority = new_pri;
                qit = queue.erase(qit);
                queues_[new_pri].push_back(std::move(item));
                if (new_pri > highest_priority_) highest_priority_ = new_pri;
            } else {
                ++qit;
            }
        }
    }
}

size_t Scheduler::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    size_t total = 0;
    for (const auto& queue : queues_) {
        total += queue.size();
    }
    return total;
}