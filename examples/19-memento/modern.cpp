// 19 Memento — the value-semantics form.
//
// In C++ a well-encapsulated *value type* is already its own memento:
//   - its fields are private, so a copy reveals nothing to the holder;
//   - its invariants are enforced on every mutation, so every copy is valid;
//   - copy-assignment restores the whole state at once.
// The caretaker just keeps copies: std::vector<CameraConfig>.
//
// This works when the originator is cheap to copy and holds no resources
// (sockets, file handles, threads, a live RTSP session). When it does, copy
// would duplicate or break the resource — then you need a separate Snapshot
// that captures only the restorable *data*, as in pattern.cpp.

#include <stdexcept>
#include <vector>

#include "check.hpp"
#include "firmware.hpp"

class CameraConfig {
public:
    void set_resolution(int w, int h) {
        if (w <= 0 || h <= 0) throw std::invalid_argument("resolution");
        width_ = w;
        height_ = h;
    }
    void set_fps(int fps) {
        if (fps < 1 || fps > 60) throw std::invalid_argument("fps");
        fps_ = fps;
    }
    void set_bitrate(int kbps) {
        if (kbps < 64) throw std::invalid_argument("bitrate");
        bitrate_kbps_ = kbps;
    }
    void set_osd(std::string text) { osd_text_ = std::move(text); }

    int width() const { return width_; }
    int height() const { return height_; }
    int fps() const { return fps_; }
    int bitrate() const { return bitrate_kbps_; }
    const std::string& osd() const { return osd_text_; }
    WireConfig to_wire() const { return {width_, height_, fps_, bitrate_kbps_, osd_text_}; }

    bool operator==(const CameraConfig&) const = default;  // regular type: copy + compare

private:
    int width_ = 1920, height_ = 1080, fps_ = 25, bitrate_kbps_ = 4000;
    std::string osd_text_ = "Lobby";
};

class ConfigPusher {
public:
    explicit ConfigPusher(FakeFirmware& fw) : fw_(fw) {}

    template <class Edit>
    bool push(CameraConfig& cfg, Edit edit) {
        CameraConfig before = cfg;  // the copy *is* the snapshot
        edit(cfg);
        if (!fw_.apply(cfg.to_wire())) {
            cfg = before;
            return false;
        }
        history_.push_back(std::move(before));
        return true;
    }

    void undo(CameraConfig& cfg) {
        if (history_.empty()) return;
        cfg = history_.back();
        history_.pop_back();
        fw_.apply(cfg.to_wire());
    }

private:
    FakeFirmware& fw_;
    std::vector<CameraConfig> history_;
};

int main() {
    FakeFirmware fw;
    CameraConfig cfg;
    const CameraConfig factory_default = cfg;
    ConfigPusher pusher(fw);

    CHECK(pusher.push(cfg, [](CameraConfig& c) { c.set_bitrate(6000); }));
    CHECK(!pusher.push(cfg, [](CameraConfig& c) {
        c.set_resolution(3840, 2160);
        c.set_osd("Lobby 4K");
    }));
    CHECK(pusher.push(cfg, [](CameraConfig& c) { c.set_fps(15); }));
    pusher.undo(cfg);

    CHECK_EQ(fw.accepted(), kAccepted);
    CHECK_EQ(fw.rejected(), kRejected);
    CHECK_EQ(cfg.width(), kFinalWidth);
    CHECK_EQ(cfg.fps(), kFinalFps);
    CHECK_EQ(cfg.bitrate(), kFinalBitrate);
    CHECK_EQ(cfg.osd(), kFinalOsd);
    CHECK(!(cfg == factory_default));  // only the bitrate push survived

    std::cout << "19-memento/modern: value copies as snapshots match pattern.cpp\n";
}
