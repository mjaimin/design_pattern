// 01 Strategy — the naive version.
//
// Recorder decides per frame whether to write it to disk. The *decision rule*
// (recording mode) is an enum, and every mode's logic and every mode's state
// lives inside Recorder.
//
// Where it hurts — try adding "record only while an AI detection is active":
//   1. add an enum value
//   2. add its state fields to Recorder (next to fields other modes ignore)
//   3. add constructor parameters every other mode must now pass or default
//   4. add a case to the switch
//   5. retest *all* modes, because they share one class and one switch
// Recorder changes for a reason that has nothing to do with recording.
// That is Divergent Change, and the switch on a type code is the classic
// smell that Strategy cures.

#include <optional>

#include "check.hpp"
#include "simulated_day.hpp"

enum class RecordingMode { Continuous, MotionTriggered, Scheduled };

class Recorder {
public:
    // Every mode's tuning knobs end up in one constructor.
    Recorder(RecordingMode mode, gw::TimestampMs post_roll_ms = 0,
             int schedule_start_hour = 0, int schedule_end_hour = 0)
        : mode_(mode),
          post_roll_ms_(post_roll_ms),
          start_hour_(schedule_start_hour),
          end_hour_(schedule_end_hour) {}

    void set_mode(RecordingMode mode) { mode_ = mode; }

    void on_frame(const gw::Frame& f) {
        bool record = false;
        switch (mode_) {
            case RecordingMode::Continuous:
                record = true;
                break;
            case RecordingMode::MotionTriggered:
                if (f.motion) last_motion_ = f.ts_ms;
                record = last_motion_ && f.ts_ms - *last_motion_ <= post_roll_ms_;
                break;
            case RecordingMode::Scheduled: {
                const int h = gw::hour_of_day(f.ts_ms);
                record = start_hour_ <= end_hour_ ? (h >= start_hour_ && h < end_hour_)
                                                  : (h >= start_hour_ || h < end_hour_);
                break;
            }
        }
        if (record) write(f);
    }

    int frames_written() const { return written_; }

private:
    void write(const gw::Frame&) { ++written_; }  // stands in for muxing to disk

    RecordingMode mode_;
    // MotionTriggered-only state:
    gw::TimestampMs post_roll_ms_;
    std::optional<gw::TimestampMs> last_motion_;
    // Scheduled-only state:
    int start_hour_;
    int end_hour_;

    int written_ = 0;
};

int run_day(Recorder& r) {
    for (const auto& f : simulated_day()) r.on_frame(f);
    return r.frames_written();
}

int main() {
    Recorder continuous(RecordingMode::Continuous);
    CHECK_EQ(run_day(continuous), kContinuous);

    Recorder night(RecordingMode::Scheduled, 0, 22, 6);
    CHECK_EQ(run_day(night), kNight);

    Recorder motion(RecordingMode::MotionTriggered, gw::at(0, 2));
    CHECK_EQ(run_day(motion), kMotion);

    std::cout << "01-strategy/naive: continuous=" << kContinuous << " night=" << kNight
              << " motion=" << kMotion << "\n";
}
