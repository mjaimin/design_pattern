// 02 Observer — the naive version.
//
// The Detector produces events and *itself* calls every component that
// cares: recorder, alarm relay, cloud uploader, live UI. It holds a reference
// to each one and knows each one's method name.
//
// Where it hurts — try adding "write every detection to an audit log":
//   1. Detector gains an #include and a constructor parameter for AuditLog
//   2. every place that constructs a Detector (main, tests, tools) changes
//   3. publish() gains another call, in an order nobody chose on purpose
//   4. the live UI is optional (a browser tab), so Detector also carries
//      "is it connected?" logic that has nothing to do with detecting
// The producer depends on all of its consumers. To unit-test detection you
// must build a recorder, a relay, an uploader and a UI. That is Inappropriate
// Intimacy plus Shotgun Surgery: every new consumer edits the producer.

#include "check.hpp"
#include "detections.hpp"

class Recorder {
public:
    void start_clip(const DetectionEvent&) { ++clips; }
    int clips = 0;
};

class AlarmRelay {
public:
    void maybe_pulse(const DetectionEvent& e) {
        if (e.label == "person" && e.confidence >= kRelayMinConfidence) ++pulses;
    }
    int pulses = 0;
};

class CloudUploader {
public:
    void upload(const DetectionEvent& e) {
        if (e.confidence >= kUploadMinConfidence) ++uploads;
    }
    int uploads = 0;
};

class LiveUi {
public:
    void push(const DetectionEvent&) {
        ++pushes;
        if (pushes == kUiMaxPushes) connected = false;  // operator closed the tab
    }
    bool connected = true;
    int pushes = 0;
};

class Detector {
public:
    // Every consumer is a constructor dependency of the producer.
    Detector(Recorder& rec, AlarmRelay& relay, CloudUploader& cloud, LiveUi& ui)
        : rec_(rec), relay_(relay), cloud_(cloud), ui_(ui) {}

    void on_inference(const DetectionEvent& e) {
        rec_.start_clip(e);
        relay_.maybe_pulse(e);
        cloud_.upload(e);
        if (ui_.connected) ui_.push(e);  // consumer lifecycle leaks into the producer
    }

private:
    Recorder& rec_;
    AlarmRelay& relay_;
    CloudUploader& cloud_;
    LiveUi& ui_;
};

int main() {
    Recorder rec;
    AlarmRelay relay;
    CloudUploader cloud;
    LiveUi ui;
    Detector detector(rec, relay, cloud, ui);

    for (const auto& e : simulated_detections()) detector.on_inference(e);

    CHECK_EQ(rec.clips, kClips);
    CHECK_EQ(relay.pulses, kPulses);
    CHECK_EQ(cloud.uploads, kUploads);
    CHECK_EQ(ui.pushes, kUiPushes);

    std::cout << "02-observer/naive: clips=" << rec.clips << " pulses=" << relay.pulses
              << " uploads=" << cloud.uploads << " ui=" << ui.pushes << "\n";
}
