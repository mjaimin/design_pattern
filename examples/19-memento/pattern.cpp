// 19 Memento — classic form.
//
// Force: the caretaker (ConfigPusher) must be able to save and restore a
//        camera configuration, but the *shape* of that configuration varies
//        (fields get added) and must stay private to CameraConfig.
//
// Roles
//   Originator : CameraConfig           — creates and restores snapshots of itself
//   Memento    : CameraConfig::Snapshot — opaque to everyone but the originator
//   Caretaker  : ConfigPusher           — holds snapshots, never looks inside
//
// Compare with naive.cpp: ConfigPusher has no Backup struct and no field list.
// Adding a field to CameraConfig touches CameraConfig only.

#include <stdexcept>
#include <vector>

#include "check.hpp"
#include "firmware.hpp"

class CameraConfig {
    // All restorable state in one private struct: snapshot = copy of it.
    struct State {
        int width = 1920, height = 1080, fps = 25, bitrate_kbps = 4000;
        std::string osd_text = "Lobby";
    };

public:
    // ---- Memento ----------------------------------------------------------
    // Narrow interface for the caretaker (copy, store, hand back), wide
    // interface for the originator (friend access to state_).
    class Snapshot {
    public:
        Snapshot(const Snapshot&) = default;
        Snapshot& operator=(const Snapshot&) = default;

    private:
        friend class CameraConfig;
        explicit Snapshot(State s) : state_(std::move(s)) {}
        State state_;
    };

    Snapshot snapshot() const { return Snapshot(state_); }
    void restore(const Snapshot& s) { state_ = s.state_; }  // whole state at once: no half-valid moment

    // ---- Normal behaviour (unchanged from naive.cpp) ---------------------
    void set_resolution(int w, int h) {
        if (w <= 0 || h <= 0) throw std::invalid_argument("resolution");
        state_.width = w;
        state_.height = h;
    }
    void set_fps(int fps) {
        if (fps < 1 || fps > 60) throw std::invalid_argument("fps");
        state_.fps = fps;
    }
    void set_bitrate(int kbps) {
        if (kbps < 64) throw std::invalid_argument("bitrate");
        state_.bitrate_kbps = kbps;
    }
    void set_osd(std::string text) { state_.osd_text = std::move(text); }

    int width() const { return state_.width; }
    int height() const { return state_.height; }
    int fps() const { return state_.fps; }
    int bitrate() const { return state_.bitrate_kbps; }
    const std::string& osd() const { return state_.osd_text; }

    WireConfig to_wire() const {
        return {state_.width, state_.height, state_.fps, state_.bitrate_kbps, state_.osd_text};
    }

private:
    State state_;
};

// ---- Caretaker ----------------------------------------------------------------
class ConfigPusher {
public:
    explicit ConfigPusher(FakeFirmware& fw) : fw_(fw) {}

    template <class Edit>
    bool push(CameraConfig& cfg, Edit edit) {
        auto before = cfg.snapshot();
        edit(cfg);
        if (!fw_.apply(cfg.to_wire())) {
            cfg.restore(before);  // rollback: the camera never accepted it
            return false;
        }
        history_.push_back(std::move(before));
        return true;
    }

    void undo(CameraConfig& cfg) {
        if (history_.empty()) return;
        cfg.restore(history_.back());
        history_.pop_back();
        fw_.apply(cfg.to_wire());
    }

    std::size_t history_depth() const { return history_.size(); }

private:
    FakeFirmware& fw_;
    std::vector<CameraConfig::Snapshot> history_;  // opaque tokens
};

int main() {
    FakeFirmware fw;
    CameraConfig cfg;
    ConfigPusher pusher(fw);

    // Same scenario as naive.cpp → same numbers.
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
    CHECK_EQ(pusher.history_depth(), 1);
    // cfg.snapshot().state_;  // does not compile: the memento is opaque outside CameraConfig

    std::cout << "19-memento/pattern: accepted=" << fw.accepted() << " rejected=" << fw.rejected()
              << " final=" << cfg.width() << "x" << cfg.height() << "@" << cfg.fps() << " "
              << cfg.bitrate() << "kbps osd=" << cfg.osd() << " history=" << pusher.history_depth()
              << "\n";
}
