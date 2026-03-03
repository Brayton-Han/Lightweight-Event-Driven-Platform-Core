#pragma once
#include <cstdint>
#include <vector>

class Metrics {
public:
    void record_latency_ns(uint64_t ns);
    void report() const;

private:
    std::vector<uint64_t> samples_;
};