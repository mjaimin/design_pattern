// 13 Chain of Responsibility — modern form: a pipeline of named predicates.
//
// When every handler has the shape "accept or reject, then the next one runs",
// the links between handlers carry no information. Replace the linked list
// with a std::vector of stages and let one loop do the forwarding:
//
//   * a stage is a name + std::function<bool(const DetectionEvent&)>
//   * the runner stops at the first stage that returns false and records
//     which one it was
//   * a chain per camera is just a different vector
//
// Trade-off vs pattern.cpp: simpler and data-driven (stages could come from
// config), but a stage can no longer do work *after* the rest of the chain has
// run (timing, "log the outcome", try/catch around downstream). For that you
// need the linked form, where a handler calls pass_on() and regains control
// when it returns — the "middleware" shape.

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "check.hpp"
#include "fixture.hpp"

struct Stage {
    std::string_view name;
    std::function<bool(const DetectionEvent&)> accept;
};

// Returns the name of the stage that rejected the event, or "dispatched".
std::string_view run(const std::vector<Stage>& chain, const DetectionEvent& ev) {
    for (const auto& stage : chain)
        if (!stage.accept(ev)) return stage.name;
    return "dispatched";
}

Stage zone_mask() {
    return {"zone", [](const DetectionEvent& ev) { return !kMaskedZones.contains(ev.zone); }};
}
Stage schedule(int from, int to) {
    return {"schedule", [=](const DetectionEvent& ev) {
                const int h = gw::hour_of_day(ev.ts);
                return from <= to ? (h >= from && h < to) : (h >= from || h < to);
            }};
}
Stage confidence(float min) {
    return {"confidence", [=](const DetectionEvent& ev) { return ev.confidence >= min; }};
}
Stage cooldown(gw::TimestampMs window) {
    // State captured in the lambda. Each call to cooldown() makes a fresh map,
    // and copying the Stage copies the map too, so chains never share memory.
    return {"cooldown",
            [window, last = std::map<std::pair<gw::CameraId, std::string>, gw::TimestampMs>{}](
                const DetectionEvent& ev) mutable {
                const auto key = std::make_pair(ev.camera, ev.label);
                if (auto it = last.find(key); it != last.end() && ev.ts - it->second < window) return false;
                last[key] = ev.ts;
                return true;
            }};
}

int main() {
    std::vector<Stage> outdoor = {zone_mask(), schedule(kArmedFrom, kArmedTo),
                                  confidence(kMinConfidence), cooldown(kCooldown)};
    std::map<std::string_view, int> outcome;
    for (const auto& ev : night_events()) ++outcome[run(outdoor, ev)];

    const Stats stats{.dispatched = outcome["dispatched"],
                      .zone = outcome["zone"],
                      .schedule = outcome["schedule"],
                      .confidence = outcome["confidence"],
                      .cooldown = outcome["cooldown"]};
    CHECK(same(stats, kExpected));

    std::vector<Stage> indoor = {zone_mask(), confidence(kMinConfidence), cooldown(kCooldown)};
    CHECK(run(indoor, kIndoorMidday) == "dispatched");

    std::cout << "13-chain-of-responsibility/modern: vector-of-stages pipeline matches pattern.cpp\n";
}
