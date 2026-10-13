// 19 Memento — the naive version.
//
// CameraConfig holds a camera's stream settings and enforces invariants in its
// setters. Before pushing a change to the camera, ConfigPusher must remember
// the old settings so it can roll back if the firmware rejects the new ones.
//
// It does that by reading every field through getters, stashing them in its
// own struct, and writing them back through setters.
//
// Where it hurts — add an `ir_mode` field to CameraConfig:
//   1. ConfigPusher::Backup needs a new member
//   2. save() and restore() each need a new line
//   3. forget either and rollback silently restores *some* of the config
// The caretaker knows the originator's full internal layout (Inappropriate
// Intimacy), and every field addition is Shotgun Surgery across two classes.
// Worse, restore() goes through validating setters one field at a time, so a
// rollback can trip an invariant halfway (e.g. a fps limit tied to resolution).

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

    // Getters exist mainly so the pusher can copy state out.
    int width() const { return width_; }
    int height() const { return height_; }
    int fps() const { return fps_; }
    int bitrate() const { return bitrate_kbps_; }
    const std::string& osd() const { return osd_text_; }

    WireConfig to_wire() const { return {width_, height_, fps_, bitrate_kbps_, osd_text_}; }

private:
    int width_ = 1920, height_ = 1080, fps_ = 25, bitrate_kbps_ = 4000;
    std::string osd_text_ = "Lobby";
};

class ConfigPusher {
public:
    explicit ConfigPusher(FakeFirmware& fw) : fw_(fw) {}

    template <class Edit>
    bool push(CameraConfig& cfg, Edit edit) {
        const Backup before = save(cfg);
        edit(cfg);
        if (!fw_.apply(cfg.to_wire())) {
            restore(cfg, before);
            return false;
        }
        history_.push_back(before);
        return true;
    }

    void undo(CameraConfig& cfg) {
        if (history_.empty()) return;
        restore(cfg, history_.back());
        history_.pop_back();
        fw_.apply(cfg.to_wire());
    }

private:
    // A mirror of CameraConfig's private fields, maintained by hand.
    struct Backup {
        int width, height, fps, bitrate;
        std::string osd;
    };
    static Backup save(const CameraConfig& c) {
        return {c.width(), c.height(), c.fps(), c.bitrate(), c.osd()};
    }
    static void restore(CameraConfig& c, const Backup& b) {
        c.set_resolution(b.width, b.height);
        c.set_fps(b.fps);
        c.set_bitrate(b.bitrate);
        c.set_osd(b.osd);
    }

    FakeFirmware& fw_;
    std::vector<Backup> history_;
};

int main() {
    FakeFirmware fw;
    CameraConfig cfg;
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

    std::cout << "19-memento/naive: accepted=" << fw.accepted() << " rejected=" << fw.rejected()
              << " final=" << cfg.width() << "x" << cfg.height() << "@" << cfg.fps() << " "
              << cfg.bitrate() << "kbps osd=" << cfg.osd() << "\n";
}
