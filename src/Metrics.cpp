#include "core/Metrics.h"
#include <iostream>
#include <algorithm>
#include <numeric>

void Metrics::record_latency_ns(uint64_t ns) { samples_.push_back(ns); }

void Metrics::report() const {
    if (samples_.empty()) {
        std::cout << "No latency samples recorded.\n";
        return;
    }

    auto v = samples_;
    std::sort(v.begin(), v.end());

    auto sum = accumulate(v.begin(), v.end(), uint64_t(0));
    auto avg = static_cast<double>(sum) / v.size();
    auto p95 = v[static_cast<size_t>(v.size() * 0.95) - 1];
    auto worst = v.back();

    std::cout << "Latency Report:\n";
    std::cout << "  Samples: " << v.size() << "\n";
    std::cout << "  Average: " << static_cast<uint64_t>(avg) << " ns\n";
    std::cout << "  95th Percentile: " << static_cast<uint64_t>(p95) << " ns\n";
    std::cout << "  Worst: " << static_cast<uint64_t>(worst) << " ns\n";
}