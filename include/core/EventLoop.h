#pragma once
#include <unordered_map>
#include <vector>
#include <functional>
#include "core/Events.h"

class EventLoop {
public:
    using Callback = std::function<void(const Event&)>;

    EventLoop();
    ~EventLoop();

    int add_timerfd(int interval_ms, Callback cb);
    int add_eventfd(Callback cb);

    void signal_eventfd(int efd, uint64_t value = 1);

    void run();
    void stop();

private:
    int epfd_{-1};
    bool running_{false};

    struct Handler {
        Callback cb;
        EventType type;
    };
    std::unordered_map<int, Handler> handlers_;
};