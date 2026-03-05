cmake_minimum_required(VERSION 3.16)
project(Lightweight-Embedded-Event-Driven-Platform-Core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
add_executable(core
    src/EventLoop.cpp
    src/main.cpp
    src/Metrics.cpp
    src/Scheduler.cpp
)

# stress test executable
add_executable(test_scheduler
    tests/test_scheduler.cpp
    src/Scheduler.cpp
)

# stress injector executable for EventLoop
add_executable(stress_injector
    tools/stress_injector.cpp
    src/EventLoop.cpp
    src/Scheduler.cpp
    src/Metrics.cpp
)

# Include directories
foreach(tgt core test_scheduler stress_injector)
    target_include_directories(${tgt} PRIVATE include)
    target_compile_options(${tgt} PRIVATE -O2 -Wall -Wextra -Wpedantic)
endforeach()