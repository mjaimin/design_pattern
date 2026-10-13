#pragma once
// Shared by naive.cpp, pattern.cpp and modern.cpp: the detection event, the
// filter configuration, a scripted batch of events and the expected outcome.
//
// Stages, in order — the first stage that rejects an event stops it:
//   zone mask   → drop detections in masked zones ("street")
//   schedule    → alert only while armed, 22:00-06:00
//   confidence  → drop below 0.6
//   cooldown    → at most one alert per (camera, label) per 30 s
//   dispatch    → send the alert

#include <set>
#include <string>
#include <vector>

#include "domain.hpp"

struct DetectionEvent {
    gw::CameraId camera;
    std::string label;
    float confidence;
    std::string zone;
    gw::TimestampMs ts;
};

inline const std::set<std::string> kMaskedZones = {"street"};
inline constexpr int kArmedFrom = 22, kArmedTo = 6;
inline constexpr float kMinConfidence = 0.6f;
inline constexpr gw::TimestampMs kCooldown = gw::at(0, 0, 30);

// Where each event ended up.
struct Stats {
    int dispatched = 0;
    int zone = 0;
    int schedule = 0;
    int confidence = 0;
    int cooldown = 0;
};

inline std::vector<DetectionEvent> night_events() {
    using gw::at;
    return {
        {"cam-1", "person", 0.90f, "entrance", at(23, 0, 0)},   // dispatched
        {"cam-1", "person", 0.95f, "entrance", at(23, 0, 10)},  // cooldown (10 s after the last alert)
        {"cam-1", "person", 0.90f, "street", at(23, 0, 20)},    // zone
        {"cam-2", "car", 0.40f, "entrance", at(23, 1, 0)},      // confidence
        {"cam-2", "car", 0.80f, "parking", at(12, 0, 0)},       // schedule (midday, disarmed)
        {"cam-1", "person", 0.90f, "entrance", at(23, 0, 45)},  // dispatched (45 s after the last alert)
        {"cam-2", "person", 0.70f, "parking", at(2, 0, 0)},     // dispatched
        {"cam-2", "person", 0.70f, "parking", at(2, 0, 5)},     // cooldown
        {"cam-3", "car", 0.30f, "street", at(1, 0, 0)},         // zone (first failing stage wins)
        {"cam-3", "car", 0.99f, "parking", at(5, 59, 59)},      // dispatched
    };
}

inline constexpr Stats kExpected{.dispatched = 4, .zone = 2, .schedule = 1, .confidence = 1, .cooldown = 2};

// An indoor camera skips the schedule stage: the midday event goes through.
inline const DetectionEvent kIndoorMidday{"cam-9", "person", 0.80f, "lobby", gw::at(12, 0, 0)};

inline bool same(const Stats& a, const Stats& b) {
    return a.dispatched == b.dispatched && a.zone == b.zone && a.schedule == b.schedule &&
           a.confidence == b.confidence && a.cooldown == b.cooldown;
}
