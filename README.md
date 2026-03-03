# Lightweight-Embedded-Event-Driven-Platform-Core

1. Designed and implemented a lightweight embedded-style event-driven platform in C++17, simulating RTOS-like
priority scheduling, interrupt handling, and deterministic dispatch under constrained resource assumptions.

2. Built a modular architecture supporting task registration, event routing, timer-driven triggers (timerfd), and
simulated hardware interrupts (eventfd).

3. Implemented a priority-based FIFO scheduler with starvation prevention (aging) and latency-aware dispatch to
ensure predictable task execution behavior.

4. Developed performance instrumentation to measure average, P95, and worst-case response latency under
configurable stress workloads.
