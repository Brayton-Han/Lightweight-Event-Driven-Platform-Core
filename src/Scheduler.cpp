#include "core/Scheduler.h"

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void Scheduler::enqueue(const Task& task, const Event& event) {
    std::lock_guard<std::mutex> lk(mu_);
    ScheduledItem item{task, event, now_ns(), task.priority};
    priority_queues_[item.effective_priority].push_back(std::move(item));
}

std::optional<ScheduledItem> Scheduler::pick_next() {
    std::lock_guard<std::mutex> lk(mu_);
    apply_aging_locked(now_ns());

    for (auto it = priority_queues_.begin(); it != priority_queues_.end(); ++it) {
        auto& queue = it->second;
        if (!queue.empty()) {
            ScheduledItem item = std::move(queue.front());
            queue.pop_front();
            if (queue.empty()) priority_queues_.erase(it);
            return item;
        }
    }
    
    return std::nullopt; // No tasks available
}

void Scheduler::apply_aging_locked(uint64_t now) {
    constexpr uint64_t aging_threshold_ns = 2'000'000; // 2ms
    std::vector<ScheduledItem> promote;
    
    for (auto it = priority_queues_.begin(); it != priority_queues_.end(); /* no increment here */) {
        auto& queue = it->second;
        for (auto qit = queue.begin(); qit != queue.end(); /* no increment here */) {
            if (now - qit->enqueue_ts_ns > aging_threshold_ns) {
                ++qit->effective_priority;
                promote.push_back(std::move(*qit));
                qit = queue.erase(qit);
            } else ++qit;
        }
        if (queue.empty()) priority_queues_.erase(it);
        else ++it;
    }

    for (auto& item : promote) {
        priority_queues_[item.effective_priority].push_back(std::move(item));
    }
}

size_t Scheduler::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    size_t total = 0;
    for (const auto& [priority, queue] : priority_queues_) {
        total += queue.size();
    }
    return total;
}