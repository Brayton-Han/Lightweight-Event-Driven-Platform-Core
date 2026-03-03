#pragma once
#include <cstdint>

enum class EventType : uint32_t {
    TimerTick = 1,
    Interrupt = 2,
};

struct Event {
    EventType type;
    uint64_t ts_ns; // Timestamp in nanoseconds
    uint64_t payload; // cnt of timerfd/eventfd
};