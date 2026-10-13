#pragma once
// Shared gateway vocabulary. Kept deliberately tiny: a type is added here only
// when a second example needs it.

#include <cstdint>
#include <string>

namespace gw {

using CameraId = std::string;

// Simulated clock: milliseconds since local midnight.
using TimestampMs = std::int64_t;

constexpr TimestampMs at(int hour, int minute = 0, int second = 0) {
    return ((hour * 60LL + minute) * 60LL + second) * 1000LL;
}

constexpr int hour_of_day(TimestampMs ts) {
    return static_cast<int>((ts / at(1)) % 24);
}

// One decoded video frame, reduced to what the examples need.
struct Frame {
    CameraId camera;
    TimestampMs ts_ms;
    bool motion;  // verdict of an upstream (simulated) motion detector
};

}  // namespace gw
