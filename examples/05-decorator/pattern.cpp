// 05 Decorator — classic form (wrappers sharing the component interface).
//
// Force: delivery concerns (retry, rate limit, metrics, logging, ...) are
//        optional, combine freely, and their *order* matters per deployment;
//        the transport and its callers must not change when a concern is
//        added, removed or reordered.
//
// Roles
//   Component          : EventSink        — send(Event) → bool
//   Concrete component : CloudSink        — the real transport, nothing else
//   Decorator base     : SinkDecorator    — *is-a* EventSink and *has-a* EventSink
//   Concrete decorators: RetrySink, RateLimitSink, MetricsSink, LoggingSink
//
// Each decorator does one thing, then forwards to the sink it wraps. The
// caller holds the outermost one and can't tell it from a bare CloudSink.

#include <map>
#include <memory>

#include "check.hpp"
#include "sink.hpp"

// ---- Component ------------------------------------------------------------------
class EventSink {
public:
    virtual ~EventSink() = default;
    virtual bool send(const Event& e) = 0;
};

// ---- Concrete component -----------------------------------------------------------
class CloudSink final : public EventSink {
public:
    explicit CloudSink(FlakyCloud& cloud) : cloud_(cloud) {}
    bool send(const Event& e) override { return cloud_.post(e); }

private:
    FlakyCloud& cloud_;
};

// ---- Decorator base: owns the wrapped sink, forwards by default ------------------
class SinkDecorator : public EventSink {
public:
    explicit SinkDecorator(std::unique_ptr<EventSink> inner) : inner_(std::move(inner)) {}
    bool send(const Event& e) override { return inner_->send(e); }

protected:
    std::unique_ptr<EventSink> inner_;
};

// ---- Concrete decorators: one concern each -----------------------------------------
class RetrySink final : public SinkDecorator {
public:
    RetrySink(std::unique_ptr<EventSink> inner, int max_attempts)
        : SinkDecorator(std::move(inner)), max_(max_attempts) {}
    bool send(const Event& e) override {
        for (int i = 0; i < max_; ++i)
            if (inner_->send(e)) return true;
        return false;
    }

private:
    int max_;
};

class RateLimitSink final : public SinkDecorator {
public:
    RateLimitSink(std::unique_ptr<EventSink> inner, int max_per_window, gw::TimestampMs window_ms)
        : SinkDecorator(std::move(inner)), max_(max_per_window), window_(window_ms) {}
    bool send(const Event& e) override {
        if (++count_[e.ts_ms / window_] > max_) return false;  // dropped, never forwarded
        return inner_->send(e);
    }

private:
    int max_;
    gw::TimestampMs window_;
    std::map<gw::TimestampMs, int> count_;
};

class MetricsSink final : public SinkDecorator {
public:
    using SinkDecorator::SinkDecorator;
    bool send(const Event& e) override {
        const bool ok = inner_->send(e);
        ++(ok ? ok_ : failed_);
        return ok;
    }
    int ok() const { return ok_; }
    int failed() const { return failed_; }

private:
    int ok_ = 0, failed_ = 0;
};

class LoggingSink final : public SinkDecorator {
public:
    LoggingSink(std::unique_ptr<EventSink> inner, std::vector<std::string>& log)
        : SinkDecorator(std::move(inner)), log_(log) {}
    bool send(const Event& e) override {
        const bool ok = inner_->send(e);
        log_.push_back(e.camera + (ok ? " ok" : " FAIL"));
        return ok;
    }

private:
    std::vector<std::string>& log_;
};

int main() {
    FlakyCloud cloud;
    std::vector<std::string> log;

    // Assemble inside-out: cloud ← retry ← rate limit ← metrics ← logging.
    auto metrics = std::make_unique<MetricsSink>(std::make_unique<RateLimitSink>(
        std::make_unique<RetrySink>(std::make_unique<CloudSink>(cloud), kMaxAttempts),
        kMaxPerWindow, kWindowMs));
    MetricsSink* metrics_view = metrics.get();  // keep a handle to read counters
    std::unique_ptr<EventSink> sink = std::make_unique<LoggingSink>(std::move(metrics), log);

    for (const auto& e : simulated_burst()) sink->send(e);  // caller sees one EventSink

    // Same numbers as naive.cpp.
    CHECK_EQ(cloud.delivered.size(), kDelivered);
    CHECK_EQ(cloud.attempts, kAttempts);
    CHECK_EQ(metrics_view->ok(), kOk);
    CHECK_EQ(metrics_view->failed(), kFailed);
    CHECK_EQ(log.size(), kLogLines);

    // Reorder: retry OUTSIDE the limiter. Now a failed attempt is retried
    // through the limiter and burns window budget — a different policy,
    // obtained by changing only the assembly line, not any class.
    FlakyCloud cloud2;
    RetrySink reordered(std::make_unique<RateLimitSink>(std::make_unique<CloudSink>(cloud2),
                                                        kMaxPerWindow, kWindowMs),
                        kMaxAttempts);
    for (const auto& e : simulated_burst()) reordered.send(e);
    CHECK(cloud2.delivered.size() < kDelivered);  // 3rd event's retry was rate-limited

    std::cout << "05-decorator/pattern: delivered=" << cloud.delivered.size()
              << " attempts=" << cloud.attempts << " ok=" << metrics_view->ok()
              << " failed=" << metrics_view->failed()
              << " | retry-outside-limiter delivered=" << cloud2.delivered.size() << "\n";
}
