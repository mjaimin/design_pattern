#pragma once
// STRATEGY (ch. 01): the recording rule varies per camera; Recorder doesn't.
// Reused shape from chapter 01, trimmed to the one policy the capstone uses
// plus continuous (two real variants, so the interface earns its keep).

#include <memory>
#include <optional>

#include "domain.hpp"

namespace mg {

class RecordingPolicy {
public:
    virtual ~RecordingPolicy() = default;
    virtual bool should_record(const gw::Frame& f) = 0;
};

class ContinuousPolicy final : public RecordingPolicy {
public:
    bool should_record(const gw::Frame&) override { return true; }
};

class MotionTriggeredPolicy final : public RecordingPolicy {
public:
    explicit MotionTriggeredPolicy(gw::TimestampMs post_roll) : post_roll_(post_roll) {}
    bool should_record(const gw::Frame& f) override {
        if (f.motion) last_motion_ = f.ts_ms;
        return last_motion_ && f.ts_ms - *last_motion_ <= post_roll_;
    }

private:
    gw::TimestampMs post_roll_;
    std::optional<gw::TimestampMs> last_motion_;
};

// One Recorder per camera, each with its *own* policy instance — a stateful
// strategy must never be shared between cameras (see 01 §4).
class Recorder {
public:
    explicit Recorder(std::unique_ptr<RecordingPolicy> p) : policy_(std::move(p)) {}
    void on_frame(const gw::Frame& f) {
        if (policy_->should_record(f)) ++written_;
    }
    int frames_written() const { return written_; }

private:
    std::unique_ptr<RecordingPolicy> policy_;
    int written_ = 0;
};

}  // namespace mg
