// 02 Observer — classic form (subject keeps a list of observer interfaces).
//
// Force: the set of components reacting to a detection grows (recorder,
//        relay, cloud, UI, audit, ...) and changes at runtime (a UI tab opens
//        and closes); the detector must not know who they are or how many.
//
// Roles
//   Subject           : Detector            — keeps the list, notifies in order
//   Observer          : DetectionObserver   — the one callback every reactor implements
//   Concrete observers: Recorder, AlarmRelay, CloudUploader, LiveUi
//
// Compare with naive.cpp: Detector has no reference to any consumer type.
// Consumers register themselves; adding an audit log touches no Detector code.

#include <algorithm>
#include <vector>

#include "check.hpp"
#include "detections.hpp"

// ---- Observer -------------------------------------------------------------
class DetectionObserver {
public:
    virtual ~DetectionObserver() = default;
    virtual void on_detection(const DetectionEvent& e) = 0;
};

// ---- Subject --------------------------------------------------------------
class Detector {
public:
    void subscribe(DetectionObserver& o) { observers_.push_back(&o); }
    void unsubscribe(DetectionObserver& o) { std::erase(observers_, &o); }

    void on_inference(const DetectionEvent& e) { notify(e); }

private:
    void notify(const DetectionEvent& e) {
        // Iterate a copy: an observer may unsubscribe (or subscribe another)
        // from inside its callback, which would invalidate a live iterator.
        const auto snapshot = observers_;
        for (auto* o : snapshot)
            if (std::ranges::find(observers_, o) != observers_.end()) o->on_detection(e);
    }

    // Non-owning: observers must unsubscribe before they die. That lifetime
    // contract is the classic Observer weakness — modern.cpp removes it.
    std::vector<DetectionObserver*> observers_;
};

// ---- Concrete observers -----------------------------------------------------
class Recorder final : public DetectionObserver {
public:
    void on_detection(const DetectionEvent&) override { ++clips; }
    int clips = 0;
};

class AlarmRelay final : public DetectionObserver {
public:
    void on_detection(const DetectionEvent& e) override {
        if (e.label == "person" && e.confidence >= kRelayMinConfidence) ++pulses;
    }
    int pulses = 0;
};

class CloudUploader final : public DetectionObserver {
public:
    void on_detection(const DetectionEvent& e) override {
        if (e.confidence >= kUploadMinConfidence) ++uploads;
    }
    int uploads = 0;
};

// The UI manages its own lifecycle: it detaches itself when the tab closes.
class LiveUi final : public DetectionObserver {
public:
    explicit LiveUi(Detector& source) : source_(source) { source_.subscribe(*this); }
    ~LiveUi() override { disconnect(); }

    void on_detection(const DetectionEvent&) override {
        if (++pushes == kUiMaxPushes) disconnect();  // unsubscribing mid-notify is safe
    }

    int pushes = 0;

private:
    void disconnect() {
        if (connected_) source_.unsubscribe(*this);
        connected_ = false;
    }
    Detector& source_;
    bool connected_ = true;
};

int main() {
    Detector detector;
    Recorder rec;
    AlarmRelay relay;
    CloudUploader cloud;
    detector.subscribe(rec);
    detector.subscribe(relay);
    detector.subscribe(cloud);
    LiveUi ui(detector);

    for (const auto& e : simulated_detections()) detector.on_inference(e);

    // Same numbers as naive.cpp.
    CHECK_EQ(rec.clips, kClips);
    CHECK_EQ(relay.pulses, kPulses);
    CHECK_EQ(cloud.uploads, kUploads);
    CHECK_EQ(ui.pushes, kUiPushes);

    // Adding a reactor touches no Detector code.
    struct AuditLog final : DetectionObserver {
        void on_detection(const DetectionEvent&) override { ++lines; }
        int lines = 0;
    } audit;
    detector.subscribe(audit);
    detector.on_inference({"cam-9", "person", 0.99f, gw::at(23, 59)});
    CHECK_EQ(audit.lines, 1);
    CHECK_EQ(ui.pushes, kUiPushes);  // UI stayed detached
    detector.unsubscribe(audit);

    std::cout << "02-observer/pattern: clips=" << kClips << " pulses=" << kPulses
              << " uploads=" << kUploads << " ui=" << kUiPushes << "\n";
}
