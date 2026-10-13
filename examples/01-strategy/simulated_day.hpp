#pragma once
// One camera, one frame per minute for 24 h (1440 frames), with motion at
// four known minutes. Shared by naive.cpp, pattern.cpp and modern.cpp so all
// three must produce identical numbers — the refactor preserves behaviour.

#include <algorithm>
#include <array>
#include <vector>

#include "domain.hpp"

inline constexpr std::array kMotionAt = {gw::at(8, 0), gw::at(8, 1), gw::at(13, 30), gw::at(23, 0)};

inline std::vector<gw::Frame> simulated_day() {
    std::vector<gw::Frame> frames;
    frames.reserve(24 * 60);
    for (int minute = 0; minute < 24 * 60; ++minute) {
        const gw::TimestampMs ts = gw::at(0, minute);
        frames.push_back({"cam-1", ts, std::ranges::find(kMotionAt, ts) != kMotionAt.end()});
    }
    return frames;
}

// Expected frames written per policy over the simulated day:
inline constexpr int kContinuous = 1440;  // every frame
inline constexpr int kNight = 8 * 60;     // 22:00-06:00 → 8 h
inline constexpr int kMotion = 4 + 3 + 3; // 2-min post-roll: 08:00-08:03, 13:30-13:32, 23:00-23:02
