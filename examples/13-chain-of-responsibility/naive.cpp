// 13 Chain of Responsibility — the naive version.
//
// One AlertPipeline::process() runs every check inline, in a fixed order,
// with flags to switch stages off for cameras that don't need them.
//
// Where it hurts:
//   * An indoor camera skips the schedule: a `use_schedule` flag. A lobby
//     camera also needs a "only during business hours" stage: another flag,
//     another `if`, and the order of checks is still hard-coded.
//   * A new stage (e.g. "drop if the same object was already alerted by a
//     neighbouring camera") means editing process() and retesting all stages.
//   * The cooldown's per-camera memory lives in the pipeline next to state
//     no other stage cares about.
//   * You cannot unit-test the confidence check without going through the
//     zone and schedule checks first.
// Smells: Long Method in the making, flag arguments (Boolean Parameters),
// and Divergent Change.

#include <map>
#include <utility>

#include "check.hpp"
#include "fixture.hpp"

class AlertPipeline {
public:
    struct Options {
        bool use_schedule = true;
    };

    AlertPipeline(Stats& stats, Options opt) : stats_(stats), opt_(opt) {}

    void process(const DetectionEvent& ev) {
        if (kMaskedZones.contains(ev.zone)) {
            ++stats_.zone;
            return;
        }
        if (opt_.use_schedule) {
            const int h = gw::hour_of_day(ev.ts);
            if (!(h >= kArmedFrom || h < kArmedTo)) {
                ++stats_.schedule;
                return;
            }
        }
        if (ev.confidence < kMinConfidence) {
            ++stats_.confidence;
            return;
        }
        const auto key = std::make_pair(ev.camera, ev.label);
        if (auto it = last_alert_.find(key); it != last_alert_.end() && ev.ts - it->second < kCooldown) {
            ++stats_.cooldown;
            return;
        }
        last_alert_[key] = ev.ts;
        ++stats_.dispatched;  // stands in for publishing the alert
    }

private:
    Stats& stats_;
    Options opt_;
    std::map<std::pair<gw::CameraId, std::string>, gw::TimestampMs> last_alert_;
};

int main() {
    Stats stats;
    AlertPipeline outdoor(stats, {});
    for (const auto& ev : night_events()) outdoor.process(ev);
    CHECK(same(stats, kExpected));

    Stats indoor_stats;
    AlertPipeline indoor(indoor_stats, {.use_schedule = false});
    indoor.process(kIndoorMidday);
    CHECK_EQ(indoor_stats.dispatched, 1);

    std::cout << "13-chain-of-responsibility/naive: dispatched=" << stats.dispatched
              << " zone=" << stats.zone << " schedule=" << stats.schedule
              << " confidence=" << stats.confidence << " cooldown=" << stats.cooldown << "\n";
}
