// 01 Strategy — two modern C++ forms of the same idea.
//
// A. std::function + lambdas  (runtime swap, no class hierarchy)
//    A strategy that is "one function, maybe with some captured state" does
//    not need an interface. The lambda *is* the concrete strategy; captured
//    variables are its state (`mutable` lets it update them).
//    Wins when: one operation, no need to ask the strategy anything else.
//    Loses when: you need several related operations (should_record + name +
//    describe_config), or want to inspect/serialise the chosen strategy.
//
// B. Template policy  (compile-time choice — "policy-based design")
//    Recorder<Policy> bakes the rule into the type. Zero dispatch cost and
//    full inlining, but no runtime swap and a different type per policy.
//    Wins when: chosen at build/config time and on a hot path (per frame,
//    per packet). This is how the standard library does it: std::map's
//    Compare, std::unique_ptr's Deleter, every container's Allocator.

#include <concepts>
#include <functional>
#include <optional>

#include "check.hpp"
#include "simulated_day.hpp"

// ---------------------------------------------------------------------------
// A. std::function
// ---------------------------------------------------------------------------
namespace function_form {

using RecordingPolicy = std::function<bool(const gw::Frame&)>;

RecordingPolicy continuous() {
    return [](const gw::Frame&) { return true; };
}

RecordingPolicy scheduled(int start, int end) {
    return [=](const gw::Frame& f) {
        const int h = gw::hour_of_day(f.ts_ms);
        return start <= end ? (h >= start && h < end) : (h >= start || h < end);
    };
}

RecordingPolicy motion_triggered(gw::TimestampMs post_roll) {
    return [post_roll, last = std::optional<gw::TimestampMs>{}](const gw::Frame& f) mutable {
        if (f.motion) last = f.ts_ms;
        return last && f.ts_ms - *last <= post_roll;
    };
}

class Recorder {
public:
    explicit Recorder(RecordingPolicy p) : policy_(std::move(p)) {}
    void set_policy(RecordingPolicy p) { policy_ = std::move(p); }
    void on_frame(const gw::Frame& f) {
        if (policy_(f)) ++written_;
    }
    int frames_written() const { return written_; }

private:
    RecordingPolicy policy_;
    int written_ = 0;
};

}  // namespace function_form

// ---------------------------------------------------------------------------
// B. Template policy, constrained by a concept
// ---------------------------------------------------------------------------
namespace template_form {

template <class P>
concept RecordingPolicy = requires(P p, const gw::Frame& f) {
    { p.should_record(f) } -> std::convertible_to<bool>;
};

struct Continuous {
    bool should_record(const gw::Frame&) const { return true; }
};

struct NightOnly {  // fixed window: compile-time constants, nothing stored
    static constexpr int start = 22, end = 6;
    bool should_record(const gw::Frame& f) const {
        const int h = gw::hour_of_day(f.ts_ms);
        return h >= start || h < end;
    }
};

struct MotionTriggered {
    gw::TimestampMs post_roll;
    std::optional<gw::TimestampMs> last{};
    bool should_record(const gw::Frame& f) {
        if (f.motion) last = f.ts_ms;
        return last && f.ts_ms - *last <= post_roll;
    }
};

template <RecordingPolicy Policy>
class Recorder {
public:
    explicit Recorder(Policy p = {}) : policy_(std::move(p)) {}
    void on_frame(const gw::Frame& f) {
        if (policy_.should_record(f)) ++written_;  // direct call, inlinable
    }
    int frames_written() const { return written_; }

private:
    Policy policy_;  // held by value: no heap, no vtable
    int written_ = 0;
};

static_assert(!RecordingPolicy<int>);

}  // namespace template_form

template <class R>
int run_day(R& r) {
    for (const auto& f : simulated_day()) r.on_frame(f);
    return r.frames_written();
}

int main() {
    using namespace function_form;
    Recorder c(continuous()), n(scheduled(22, 6)), m(motion_triggered(gw::at(0, 2)));
    CHECK_EQ(run_day(c), kContinuous);
    CHECK_EQ(run_day(n), kNight);
    CHECK_EQ(run_day(m), kMotion);

    template_form::Recorder<template_form::Continuous> tc;
    template_form::Recorder<template_form::NightOnly> tn;
    template_form::Recorder<template_form::MotionTriggered> tm({gw::at(0, 2)});
    CHECK_EQ(run_day(tc), kContinuous);
    CHECK_EQ(run_day(tn), kNight);
    CHECK_EQ(run_day(tm), kMotion);
    // tc = tn;  // does not compile: different policy = different type

    std::cout << "01-strategy/modern: std::function and template forms match pattern.cpp\n";
}
