#pragma once
// Detection events: produced by the detector, filtered by a CHAIN OF
// RESPONSIBILITY (ch. 13), then fanned out by an OBSERVER event bus (ch. 02).

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "domain.hpp"

namespace mg {

struct DetectionEvent {
    gw::CameraId camera;
    std::string label;
    float confidence;
    std::string zone;
    gw::TimestampMs ts_ms;
};

// ---- Chain of Responsibility -------------------------------------------------
// Force: the set and order of filters is configuration; the detector and the
// bus must not change when a filter is added or reordered. Any link may drop.
class EventFilter {
public:
    virtual ~EventFilter() = default;
    EventFilter& then(std::unique_ptr<EventFilter> next) {
        next_ = std::move(next);
        return *next_;
    }
    bool pass(const DetectionEvent& e) {
        if (!accept(e)) return false;
        return next_ ? next_->pass(e) : true;
    }

protected:
    virtual bool accept(const DetectionEvent& e) = 0;

private:
    std::unique_ptr<EventFilter> next_;
};

class MinConfidence final : public EventFilter {
public:
    explicit MinConfidence(float min) : min_(min) {}

protected:
    bool accept(const DetectionEvent& e) override { return e.confidence >= min_; }

private:
    float min_;
};

class ZoneMask final : public EventFilter {
public:
    explicit ZoneMask(std::set<std::string> masked) : masked_(std::move(masked)) {}

protected:
    bool accept(const DetectionEvent& e) override { return !masked_.contains(e.zone); }

private:
    std::set<std::string> masked_;
};

// Suppress repeats of the same label on the same camera inside a window.
class Cooldown final : public EventFilter {
public:
    explicit Cooldown(gw::TimestampMs window) : window_(window) {}

protected:
    bool accept(const DetectionEvent& e) override {
        const auto key = std::make_pair(e.camera, e.label);
        auto it = last_.find(key);
        if (it != last_.end() && e.ts_ms - it->second < window_) return false;
        last_[key] = e.ts_ms;
        return true;
    }

private:
    gw::TimestampMs window_;
    std::map<std::pair<gw::CameraId, std::string>, gw::TimestampMs> last_;
};

// ---- Observer -------------------------------------------------------------------
// Force: the number of reactors to an event grows (recorder bookmarks, relays,
// cloud upload, UI); the publisher must not know any of them.
// C++ detail: subscriptions are RAII tokens, so a destroyed subscriber can never
// leave a dangling callback behind (see 92 anti-patterns).
class EventBus {
public:
    using Handler = std::function<void(const DetectionEvent&)>;

    class Subscription {
    public:
        Subscription(EventBus* bus, int id) : bus_(bus), id_(id) {}
        Subscription(Subscription&& o) noexcept : bus_(std::exchange(o.bus_, nullptr)), id_(o.id_) {}
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        Subscription& operator=(Subscription&&) = delete;
        ~Subscription() {
            if (bus_) bus_->handlers_.erase(id_);
        }

    private:
        EventBus* bus_;
        int id_;
    };

    [[nodiscard]] Subscription subscribe(Handler h) {
        handlers_.emplace(next_id_, std::move(h));
        return Subscription(this, next_id_++);
    }

    void publish(const DetectionEvent& e) const {
        for (const auto& [id, h] : handlers_) h(e);
    }

    std::size_t subscriber_count() const { return handlers_.size(); }

private:
    std::map<int, Handler> handlers_;
    int next_id_ = 0;
};

}  // namespace mg
