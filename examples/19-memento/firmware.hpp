#pragma once
// A simulated camera firmware that accepts or rejects a stream configuration.
// Shared by naive.cpp, pattern.cpp and modern.cpp so all three run the same
// push sequence against the same rules.

#include <string>

struct WireConfig {  // what actually goes over the wire to the camera
    int width;
    int height;
    int fps;
    int bitrate_kbps;
    std::string osd_text;
};

class FakeFirmware {
public:
    bool apply(const WireConfig& c) {
        const bool resolution_ok = (c.width == 1920 && c.height == 1080) ||
                                   (c.width == 1280 && c.height == 720);
        const bool ok = resolution_ok && c.fps <= 30 && c.bitrate_kbps <= 8000;
        ok ? ++accepted_ : ++rejected_;
        return ok;
    }
    int accepted() const { return accepted_; }
    int rejected() const { return rejected_; }

private:
    int accepted_ = 0;
    int rejected_ = 0;
};

// Scenario (identical in every file):
//   start   1920x1080 @25fps, 4000 kbps, OSD "Lobby"
//   push 1  bitrate → 6000                       accepted
//   push 2  resolution → 3840x2160, OSD "Lobby 4K" rejected → roll back
//   push 3  fps → 15                              accepted
//   undo    last accepted change                  fps back to 25
inline constexpr int kAccepted = 3;  // push 1, push 3, and the undo's re-push
inline constexpr int kRejected = 1;
inline constexpr int kFinalWidth = 1920;
inline constexpr int kFinalFps = 25;
inline constexpr int kFinalBitrate = 6000;
inline const std::string kFinalOsd = "Lobby";
