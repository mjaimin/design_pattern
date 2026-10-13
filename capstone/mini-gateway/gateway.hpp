#pragma once
// FACADE (ch. 04): one class that wires cameras, recorders, the filter chain,
// the event bus and the relay command queue. Callers see add_camera / subscribe
// / run — not the six subsystems behind them.

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "camera.hpp"
#include "events.hpp"
#include "recording.hpp"
#include "relays.hpp"

namespace mg {

// NO PATTERN: configuration is a plain aggregate with defaults, built with
// designated initializers. Six fields, all optional, no invalid combinations —
// a Builder would add a class and buy nothing.
struct GatewayConfig {
    float min_confidence = 0.5f;
    std::set<std::string> masked_zones{};
    gw::TimestampMs cooldown = gw::at(0, 0, 30);
    gw::TimestampMs post_roll = gw::at(0, 0, 2);
    std::string alarm_zone = "entrance";
    int alarm_relay = 1;
};

// NO PATTERN: the detector is a single injected function, not a Detector
// hierarchy. There is one detector; std::function is the lightest seam that
// still lets the simulation (or a test) supply it.
using Detector = std::function<std::optional<DetectionEvent>(const gw::Frame&)>;

class Gateway {
public:
    Gateway(GatewayConfig cfg, Detector detect)
        : cfg_(std::move(cfg)),
          detect_(std::move(detect)),
          filters_(std::make_unique<MinConfidence>(cfg_.min_confidence)),
          // Built-in reactor: person in the alarm zone → queue a relay pulse.
          alarm_rule_(bus_.subscribe([this](const DetectionEvent& e) {
              if (e.label == "person" && e.zone == cfg_.alarm_zone)
                  commands_.push(std::make_unique<PulseRelay>(
                      cfg_.alarm_relay, gw::at(0, 0, 5), e.camera + " person at " + e.zone));
          })) {
        filters_->then(std::make_unique<ZoneMask>(cfg_.masked_zones))
            .then(std::make_unique<Cooldown>(cfg_.cooldown));
    }

    // Captures `this` in a subscription: copying or moving would dangle it.
    Gateway(const Gateway&) = delete;
    Gateway& operator=(const Gateway&) = delete;

    void add_camera(std::unique_ptr<Camera> cam) {
        // Fresh policy per camera — stateful strategies are never shared.
        channels_.push_back({std::move(cam),
                             Recorder(std::make_unique<MotionTriggeredPolicy>(cfg_.post_roll))});
    }

    [[nodiscard]] EventBus::Subscription subscribe(EventBus::Handler h) {
        return bus_.subscribe(std::move(h));
    }

    // Round-robin one frame per camera per tick until every stream ends;
    // queued relay commands execute once per tick.
    void run() {
        for (bool any = true; any;) {
            any = false;
            for (auto& ch : channels_) {
                if (ch.done) continue;
                auto frame = ch.camera->next_frame();
                if (!frame) {
                    ch.done = true;
                    continue;
                }
                any = true;
                ch.recorder.on_frame(*frame);
                if (auto ev = detect_(*frame); ev && filters_->pass(*ev)) bus_.publish(*ev);
            }
            commands_.drain(relays_);
        }
    }

    int frames_recorded(const gw::CameraId& id) const {
        for (const auto& ch : channels_)
            if (ch.camera->id() == id) return ch.recorder.frames_written();
        return -1;
    }
    int relay_activations() const { return relays_.activations(cfg_.alarm_relay); }
    const std::vector<std::string>& action_log() const { return commands_.log(); }

private:
    struct Channel {
        std::unique_ptr<Camera> camera;
        Recorder recorder;
        bool done = false;
    };

    // Declaration order matters: alarm_rule_ unsubscribes from bus_ in its
    // destructor, so it must be destroyed first (declared last).
    GatewayConfig cfg_;
    Detector detect_;
    std::unique_ptr<EventFilter> filters_;
    RelayBoard relays_;
    CommandQueue commands_;
    std::vector<Channel> channels_;
    EventBus bus_;
    EventBus::Subscription alarm_rule_;
};

}  // namespace mg
