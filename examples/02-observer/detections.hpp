#pragma once
// A detector's output for one simulated day, plus what every reactor should
// have done with it. Shared by naive.cpp, pattern.cpp and modern.cpp so all
// three must produce identical numbers.

#include <string>
#include <vector>

#include "domain.hpp"

struct DetectionEvent {
    gw::CameraId camera;
    std::string label;
    float confidence;
    gw::TimestampMs ts_ms;
};

inline std::vector<DetectionEvent> simulated_detections() {
    return {
        {"cam-1", "person", 0.92f, gw::at(8, 0)},
        {"cam-1", "car", 0.65f, gw::at(8, 5)},
        {"cam-2", "person", 0.40f, gw::at(9, 0)},
        {"cam-2", "person", 0.85f, gw::at(22, 30)},
        {"cam-3", "dog", 0.55f, gw::at(23, 10)},
    };
}

// Reactor rules and expected counts over simulated_detections():
inline constexpr float kRelayMinConfidence = 0.8f;   // relay: confident "person" only
inline constexpr float kUploadMinConfidence = 0.5f;  // cloud: anything reasonably sure
inline constexpr int kUiMaxPushes = 3;               // UI tab closes after 3 pushes

inline constexpr int kClips = 5;    // recorder: a clip for every detection
inline constexpr int kPulses = 2;   // 08:00 (0.92) and 22:30 (0.85)
inline constexpr int kUploads = 4;  // all but the 0.40 one
inline constexpr int kUiPushes = 3; // then the UI unsubscribes
