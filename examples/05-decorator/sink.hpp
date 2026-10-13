#pragma once
// Event delivery to the cloud, simulated deterministically so naive.cpp,
// pattern.cpp and modern.cpp must produce identical numbers.
//
//   FlakyCloud     — the real transport: every 3rd attempt fails (timeout)
//   simulated_burst— 10 events: 5 inside one second, then 5 spread over ~1.2 s

#include <string>
#include <vector>

#include "domain.hpp"

struct Event {
    gw::CameraId camera;
    std::string label;
    gw::TimestampMs ts_ms;
};

class FlakyCloud {
public:
    bool post(const Event& e) {
        if (++attempts % 3 == 0) return false;  // simulated timeout
        delivered.push_back(e);
        return true;
    }
    int attempts = 0;
    std::vector<Event> delivered;
};

inline std::vector<Event> simulated_burst() {
    std::vector<Event> v;
    for (int i = 0; i < 5; ++i) v.push_back({"cam-1", "person", gw::at(8, 0, 0) + i * 100});
    for (int i = 0; i < 5; ++i) v.push_back({"cam-2", "car", gw::at(8, 0, 5) + i * 300});
    return v;
}

// Policy knobs shared by all three versions:
inline constexpr int kMaxAttempts = 3;            // retry: up to 3 tries per event
inline constexpr int kMaxPerWindow = 3;           // rate limit: 3 events ...
inline constexpr gw::TimestampMs kWindowMs = 1000; // ... per 1 s fixed window

// Expected, with stack  log( metrics( rate_limit( retry( cloud ))))
//   rate limit: window 08:00:00 → 3 pass, 2 drop
//               window 08:00:05 → 3 pass (.000 .300 .600), 1 drop (.900)
//               window 08:00:06 → 1 pass (.200)
//   → 7 reach retry; attempts 3, 6, 9 fail once and succeed on retry
inline constexpr int kDelivered = 7;
inline constexpr int kAttempts = 10;
inline constexpr int kOk = 7;
inline constexpr int kFailed = 3;     // the rate-limited drops
inline constexpr int kLogLines = 10;  // one per send() at the outermost layer
