// 05 Decorator — modern form: higher-order functions over std::function.
//
// When the component interface is a single operation, a decorator is just a
// function that takes a sink and returns a sink. No base class, no
// SinkDecorator, no unique_ptr chains: each concern is a factory function
// returning a lambda that captures the inner sink by value.
//
//   Sink with_retry(Sink inner, int n);
//   Sink with_rate_limit(Sink inner, int max, TimestampMs window);
//   auto sink = with_logging(with_metrics(with_rate_limit(with_retry(cloud, 3), 3, 1000), m), log);
//
// This is the same composition you know from middleware stacks (HTTP
// handlers wrapping handlers). It loses what the class form gives: you can't
// ask a std::function for its counters, so observable state (metrics, log)
// lives outside and is captured by reference.

#include <functional>
#include <map>
#include <memory>

#include "check.hpp"
#include "sink.hpp"

using Sink = std::function<bool(const Event&)>;

Sink with_retry(Sink inner, int max_attempts) {
    return [inner = std::move(inner), max_attempts](const Event& e) {
        for (int i = 0; i < max_attempts; ++i)
            if (inner(e)) return true;
        return false;
    };
}

Sink with_rate_limit(Sink inner, int max_per_window, gw::TimestampMs window_ms) {
    // std::function must be copyable, so mutable state goes behind a shared_ptr.
    auto count = std::make_shared<std::map<gw::TimestampMs, int>>();
    return [inner = std::move(inner), count, max_per_window, window_ms](const Event& e) {
        if (++(*count)[e.ts_ms / window_ms] > max_per_window) return false;
        return inner(e);
    };
}

struct Metrics {
    int ok = 0, failed = 0;
};

Sink with_metrics(Sink inner, Metrics& m) {
    return [inner = std::move(inner), &m](const Event& e) {
        const bool ok = inner(e);
        ++(ok ? m.ok : m.failed);
        return ok;
    };
}

Sink with_logging(Sink inner, std::vector<std::string>& log) {
    return [inner = std::move(inner), &log](const Event& e) {
        const bool ok = inner(e);
        log.push_back(e.camera + (ok ? " ok" : " FAIL"));
        return ok;
    };
}

int main() {
    FlakyCloud cloud;
    Metrics metrics;
    std::vector<std::string> log;

    Sink cloud_sink = [&cloud](const Event& e) { return cloud.post(e); };
    Sink sink = with_logging(
        with_metrics(with_rate_limit(with_retry(cloud_sink, kMaxAttempts), kMaxPerWindow, kWindowMs),
                     metrics),
        log);

    for (const auto& e : simulated_burst()) sink(e);

    CHECK_EQ(cloud.delivered.size(), kDelivered);
    CHECK_EQ(cloud.attempts, kAttempts);
    CHECK_EQ(metrics.ok, kOk);
    CHECK_EQ(metrics.failed, kFailed);
    CHECK_EQ(log.size(), kLogLines);

    std::cout << "05-decorator/modern: function decorators match pattern.cpp\n";
}
