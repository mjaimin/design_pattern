// 01 Strategy — classic form (interface + virtual dispatch).
//
// Force: the rule "should this frame be recorded?" varies per camera and can be
//        changed at runtime by an operator; the recorder loop (receive frame →
//        decide → write) must not change when a new rule is added.
//
// Roles
//   Context   : Recorder         — owns a policy, calls it, never asks which one
//   Strategy  : RecordingPolicy  — the one decision, as an interface
//   Concretes : ContinuousPolicy, ScheduledPolicy, MotionTriggeredPolicy
//
// Compare with naive.cpp: each policy's knobs and state now live in its own
// class; Recorder has no enum, no switch, and no fields it doesn't use.

#include <memory>
#include <optional>
#include <string_view>

#include "check.hpp"
#include "simulated_day.hpp"

// ---- Strategy -------------------------------------------------------------
class RecordingPolicy {
public:
    virtual ~RecordingPolicy() = default;
    // Non-const on purpose: some policies remember things (last motion).
    // Consequence: one policy instance per camera — never share a stateful one.
    virtual bool should_record(const gw::Frame& f) = 0;
    virtual std::string_view name() const = 0;
};

// ---- Concrete strategies ---------------------------------------------------
class ContinuousPolicy final : public RecordingPolicy {
public:
    bool should_record(const gw::Frame&) override { return true; }
    std::string_view name() const override { return "continuous"; }
};

// Record within [start_hour, end_hour); handles windows that wrap midnight.
class ScheduledPolicy final : public RecordingPolicy {
public:
    ScheduledPolicy(int start_hour, int end_hour) : start_(start_hour), end_(end_hour) {}

    bool should_record(const gw::Frame& f) override {
        const int h = gw::hour_of_day(f.ts_ms);
        return start_ <= end_ ? (h >= start_ && h < end_) : (h >= start_ || h < end_);
    }
    std::string_view name() const override { return "scheduled"; }

private:
    int start_;
    int end_;
};

// Record while motion is seen, plus a post-roll after the last motion.
class MotionTriggeredPolicy final : public RecordingPolicy {
public:
    explicit MotionTriggeredPolicy(gw::TimestampMs post_roll_ms) : post_roll_(post_roll_ms) {}

    bool should_record(const gw::Frame& f) override {
        if (f.motion) last_motion_ = f.ts_ms;
        return last_motion_ && f.ts_ms - *last_motion_ <= post_roll_;
    }
    std::string_view name() const override { return "motion"; }

private:
    gw::TimestampMs post_roll_;
    std::optional<gw::TimestampMs> last_motion_;
};

// ---- Context ----------------------------------------------------------------
class Recorder {
public:
    explicit Recorder(std::unique_ptr<RecordingPolicy> policy) : policy_(std::move(policy)) {}

    // Runtime swap — the operator changes the camera's mode from the UI.
    void set_policy(std::unique_ptr<RecordingPolicy> policy) { policy_ = std::move(policy); }

    void on_frame(const gw::Frame& f) {
        if (policy_->should_record(f)) write(f);
    }

    int frames_written() const { return written_; }
    std::string_view policy_name() const { return policy_->name(); }

private:
    void write(const gw::Frame&) { ++written_; }  // stands in for muxing to disk

    std::unique_ptr<RecordingPolicy> policy_;
    int written_ = 0;
};

int run_day(Recorder& r) {
    for (const auto& f : simulated_day()) r.on_frame(f);
    return r.frames_written();
}

int main() {
    // Same numbers as naive.cpp: the refactor did not change behaviour.
    Recorder continuous(std::make_unique<ContinuousPolicy>());
    CHECK_EQ(run_day(continuous), kContinuous);

    Recorder night(std::make_unique<ScheduledPolicy>(22, 6));
    CHECK_EQ(run_day(night), kNight);

    Recorder motion(std::make_unique<MotionTriggeredPolicy>(gw::at(0, 2)));
    CHECK_EQ(run_day(motion), kMotion);

    // Runtime swap: continuous until noon, then motion-triggered.
    // Before noon: 720 frames. After: motion at 13:30 (3) and 23:00 (3).
    Recorder swapped(std::make_unique<ContinuousPolicy>());
    for (const auto& f : simulated_day()) {
        if (f.ts_ms == gw::at(12)) swapped.set_policy(std::make_unique<MotionTriggeredPolicy>(gw::at(0, 2)));
        swapped.on_frame(f);
    }
    CHECK_EQ(swapped.frames_written(), 720 + 3 + 3);
    CHECK(swapped.policy_name() == "motion");

    std::cout << "01-strategy/pattern: continuous=" << kContinuous << " night=" << kNight
              << " motion=" << kMotion << " swapped=" << swapped.frames_written() << "\n";
}
