#pragma once
// The gateway's camera inventory (as it would come from config) and what a
// health sweep over it must produce. Shared by naive.cpp and pattern.cpp.

#include <string>
#include <vector>

struct CameraConfig {
    std::string vendor;  // "vendor_a", "vendor_b", "onvif"
    std::string host;
    int channel = 1;     // only vendor_b uses it
};

inline std::vector<CameraConfig> inventory() {
    return {
        {"vendor_a", "10.0.0.11"},
        {"vendor_b", "10.0.0.12", 2},
        {"vendor_b", "10.0.0.99"},  // unplugged
        {"onvif", "10.0.0.13"},
    };
}

// Expected sweep result:
inline const std::vector<std::string> kExpectedUrls = {
    "rtsp://10.0.0.11:554/stream1",
    "rtsp://10.0.0.12:554/h264/ch2/main",
    "rtsp://10.0.0.13:554/onvif/profile_h264",  // H.264 profile, not JPEG
};
inline const std::vector<std::string> kExpectedModels = {"A-200", "B-NVR-8", "", "ONVIF-Cam-X"};
inline constexpr int kOnline = 3;
inline constexpr int kRebooted = 3;
