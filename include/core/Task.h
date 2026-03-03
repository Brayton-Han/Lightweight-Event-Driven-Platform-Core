#pragma once
#include <functional>
#include <string>
#include "core/Events.h"

struct Task {
    int priority;  // Task priority (greater value means higher priority)
    std::string name; // Task name for identification
    std::function<void(const Event&)> handler; // Function to handle events for this task
};