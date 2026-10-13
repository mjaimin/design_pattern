// 02 Observer — modern form: a typed Signal with RAII connections.
//
// The classic form has two weaknesses:
//   1. every reactor must inherit DetectionObserver (a lambda or a free
//      function can't subscribe);
//   2. the subject holds raw pointers, so an observer destroyed without
//      unsubscribing leaves a dangling pointer — a use-after-free on the next
//      notify.
//
// Signal<Args...> fixes both: slots are std::function, and connect() returns
// a Connection handle that disconnects in its destructor. Lifetime of the
// subscription == lifetime of the handle. This is the shape of
// Boost.Signals2 (scoped_connection) and Qt's signals/slots.

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "check.hpp"
#include "detections.hpp"

template <class... Args>
class Signal {
    struct Slot {
        std::uint64_t id;
        std::function<void(Args...)> fn;
    };
    struct State {
        std::vector<Slot> slots;
        std::uint64_t next_id = 0;
    };

public:
    // Move-only RAII handle. Holds a weak_ptr, so it is also safe when the
    // Signal dies first.
    class Connection {
    public:
        Connection() = default;
        Connection(std::weak_ptr<State> s, std::uint64_t id) : state_(std::move(s)), id_(id) {}
        Connection(Connection&& o) noexcept : state_(std::move(o.state_)), id_(o.id_) {}
        Connection& operator=(Connection&& o) noexcept {
            if (this != &o) {
                disconnect();
                state_ = std::move(o.state_);
                id_ = o.id_;
            }
            return *this;
        }
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        ~Connection() { disconnect(); }

        void disconnect() {
            if (auto s = state_.lock())
                std::erase_if(s->slots, [id = id_](const Slot& sl) { return sl.id == id; });
            state_.reset();
        }

    private:
        std::weak_ptr<State> state_;
        std::uint64_t id_ = 0;
    };

    [[nodiscard]] Connection connect(std::function<void(Args...)> fn) {
        const auto id = state_->next_id++;
        state_->slots.push_back({id, std::move(fn)});
        return Connection(state_, id);
    }

    void emit(const Args&... args) {
        const auto snapshot = state_->slots;  // slots may disconnect while running
        for (const auto& s : snapshot) {
            const bool still_connected = std::ranges::any_of(
                state_->slots, [&](const Slot& live) { return live.id == s.id; });
            if (still_connected) s.fn(args...);
        }
    }

private:
    std::shared_ptr<State> state_ = std::make_shared<State>();
};

class Detector {
public:
    Signal<DetectionEvent> detected;  // the subject *is* the signal it exposes
    void on_inference(const DetectionEvent& e) { detected.emit(e); }
};

int main() {
    Detector detector;
    int clips = 0, pulses = 0, uploads = 0, ui = 0;

    // Reactors are plain lambdas — no base class.
    auto c1 = detector.detected.connect([&](const DetectionEvent&) { ++clips; });
    auto c2 = detector.detected.connect([&](const DetectionEvent& e) {
        if (e.label == "person" && e.confidence >= kRelayMinConfidence) ++pulses;
    });
    auto c3 = detector.detected.connect([&](const DetectionEvent& e) {
        if (e.confidence >= kUploadMinConfidence) ++uploads;
    });
    Signal<DetectionEvent>::Connection ui_conn;
    ui_conn = detector.detected.connect([&](const DetectionEvent&) {
        if (++ui == kUiMaxPushes) ui_conn.disconnect();  // tab closed
    });

    for (const auto& e : simulated_detections()) detector.on_inference(e);

    CHECK_EQ(clips, kClips);
    CHECK_EQ(pulses, kPulses);
    CHECK_EQ(uploads, kUploads);
    CHECK_EQ(ui, kUiPushes);

    // Scope-bound subscription: the handle's destructor unsubscribes, so a
    // reactor that goes away can never be called again.
    int audit = 0;
    {
        auto tmp = detector.detected.connect([&](const DetectionEvent&) { ++audit; });
        detector.on_inference({"cam-9", "person", 0.99f, gw::at(23, 59)});
    }
    detector.on_inference({"cam-9", "person", 0.99f, gw::at(23, 59)});
    CHECK_EQ(audit, 1);
    CHECK_EQ(clips, kClips + 2);

    std::cout << "02-observer/modern: Signal + RAII Connection matches pattern.cpp\n";
}
