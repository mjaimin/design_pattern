// 05 Decorator — the naive version.
//
// One CloudSink class does delivery *and* every optional concern, each
// switched on by a flag: logging, metrics, rate limiting, retry.
//
// Where it hurts:
//   - 4 flags = 16 combinations living in one method; each new concern
//     (encryption, batching, circuit breaker) adds a flag, config fields and
//     another block inside send()
//   - the *order* of concerns is frozen in code. Should a retry consume
//     rate-limit budget? Here it can't, because retry sits inside the
//     limiter — and changing that means rewriting send()
//   - you can't reuse "retry" for the MQTT sink without copying it
//   - unit-testing the rate limiter means constructing a cloud transport
// Large Class + a flag-driven Long Method; concerns can't be composed.

#include <map>

#include "check.hpp"
#include "sink.hpp"

struct CloudSinkOptions {
    bool logging = false;
    bool metrics = false;
    bool rate_limit = false;
    int max_per_window = 0;
    gw::TimestampMs window_ms = 0;
    bool retry = false;
    int max_attempts = 1;
};

class CloudSink {
public:
    CloudSink(FlakyCloud& cloud, CloudSinkOptions opt) : cloud_(cloud), opt_(opt) {}

    bool send(const Event& e) {
        bool ok = true;
        if (opt_.rate_limit) {
            const auto window = e.ts_ms / opt_.window_ms;
            if (++per_window_[window] > opt_.max_per_window) ok = false;
        }
        if (ok) {
            const int tries = opt_.retry ? opt_.max_attempts : 1;
            ok = false;
            for (int i = 0; i < tries && !ok; ++i) ok = cloud_.post(e);
        }
        if (opt_.metrics) ++(ok ? sent_ok : sent_failed);
        if (opt_.logging) log.push_back(e.camera + (ok ? " ok" : " FAIL"));
        return ok;
    }

    int sent_ok = 0, sent_failed = 0;
    std::vector<std::string> log;

private:
    FlakyCloud& cloud_;
    CloudSinkOptions opt_;
    std::map<gw::TimestampMs, int> per_window_;
};

int main() {
    FlakyCloud cloud;
    CloudSink sink(cloud, {.logging = true,
                           .metrics = true,
                           .rate_limit = true,
                           .max_per_window = kMaxPerWindow,
                           .window_ms = kWindowMs,
                           .retry = true,
                           .max_attempts = kMaxAttempts});

    for (const auto& e : simulated_burst()) sink.send(e);

    CHECK_EQ(cloud.delivered.size(), kDelivered);
    CHECK_EQ(cloud.attempts, kAttempts);
    CHECK_EQ(sink.sent_ok, kOk);
    CHECK_EQ(sink.sent_failed, kFailed);
    CHECK_EQ(sink.log.size(), kLogLines);

    std::cout << "05-decorator/naive: delivered=" << cloud.delivered.size()
              << " attempts=" << cloud.attempts << " ok=" << sink.sent_ok
              << " failed=" << sink.sent_failed << "\n";
}
