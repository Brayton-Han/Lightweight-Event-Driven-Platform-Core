#include "core/EventLoop.h"
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <stdexcept>

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

EventLoop::EventLoop() {
    epfd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ < 0) throw std::runtime_error("Failed to create epoll instance");
}

EventLoop::~EventLoop() {
    if (epfd_ >= 0) close(epfd_);
    for (const auto& [fd, _] : handlers_) {
        close(fd);
    }
}

int EventLoop::add_timerfd(int interval_ms, Callback cb) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) throw std::runtime_error("Failed to create timerfd");

    itimerspec its{};
    its.it_interval.tv_sec = interval_ms / 1000;
    its.it_interval.tv_nsec = (interval_ms % 1000) * 1'000'000;
    its.it_value = its.it_interval; // Start immediately
    if (timerfd_settime(tfd, 0, &its, nullptr) < 0) {
        close(tfd);
        throw std::runtime_error("Failed to set timerfd");
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, tfd, &ev) < 0) {
        close(tfd);
        throw std::runtime_error("Failed to add timerfd to epoll");
    }

    handlers_[tfd] = Handler{std::move(cb), EventType::TimerTick};
    return tfd;
}

int EventLoop::add_eventfd(Callback cb) {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) throw std::runtime_error("Failed to create eventfd");

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = efd;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, efd, &ev) < 0) {
        close(efd);
        throw std::runtime_error("Failed to add eventfd to epoll");
    }

    handlers_[efd] = Handler{std::move(cb), EventType::Interrupt};
    return efd;
}

void EventLoop::signal_eventfd(int efd, uint64_t value) {
    if (write(efd, &value, sizeof(value)) < 0) {
        throw std::runtime_error("Failed to signal eventfd");
    }
}

void EventLoop::run() {
    running_ = true;
    std::vector<epoll_event> events(64);

    while (running_) {
        int n = epoll_wait(epfd_, events.data(), static_cast<int>(events.size()), 50);
        if (n < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            throw std::runtime_error("Failed to wait on epoll");
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            auto it = handlers_.find(fd);
            if (it == handlers_.end()) continue;

            uint64_t val = 0;
            if (read(fd, &val, sizeof(val)) <= 0) continue;

            Event e{it->second.type, now_ns(), val};
            it->second.cb(e);
        }
    }
}

void EventLoop::stop() { running_ = false; }