// 13 Chain of Responsibility — classic form (linked handlers).
//
// Force: the checks a detection event goes through, and their order, vary per
//        camera (indoor cameras skip the schedule; some sites add stages),
//        and any check may stop the event. The detector that emits events and
//        the dispatcher at the end must not change when a check is added,
//        removed or reordered.
//
// Roles
//   Handler          : EventFilter — handle(ev); holds the next handler
//   Concrete handlers: ZoneMaskFilter, ScheduleFilter, ConfidenceFilter,
//                      CooldownFilter, Dispatcher (end of chain)
//   Client           : builds the chain per camera and feeds it events
//
// Each handler decides one thing: stop the event here, or pass it on.
// It does not know which handler is next, or how many there are.

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "check.hpp"
#include "fixture.hpp"

// ---- Handler -------------------------------------------------------------------
class EventFilter {
public:
    virtual ~EventFilter() = default;

    // Appends a handler and returns it, so chains read left to right:
    // a.then(b).then(c)
    EventFilter& then(std::unique_ptr<EventFilter> next) {
        next_ = std::move(next);
        return *next_;
    }

    virtual void handle(const DetectionEvent& ev) = 0;

protected:
    void pass_on(const DetectionEvent& ev) {
        if (next_) next_->handle(ev);
    }

private:
    std::unique_ptr<EventFilter> next_;
};

// ---- Concrete handlers -------------------------------------------------------
class ZoneMaskFilter final : public EventFilter {
public:
    ZoneMaskFilter(std::set<std::string> masked, Stats& s) : masked_(std::move(masked)), stats_(s) {}
    void handle(const DetectionEvent& ev) override {
        if (masked_.contains(ev.zone)) {
            ++stats_.zone;
            return;  // stop: the rest of the chain never sees it
        }
        pass_on(ev);
    }

private:
    std::set<std::string> masked_;
    Stats& stats_;
};

class ScheduleFilter final : public EventFilter {
public:
    ScheduleFilter(int from_hour, int to_hour, Stats& s) : from_(from_hour), to_(to_hour), stats_(s) {}
    void handle(const DetectionEvent& ev) override {
        const int h = gw::hour_of_day(ev.ts);
        const bool armed = from_ <= to_ ? (h >= from_ && h < to_) : (h >= from_ || h < to_);
        if (!armed) {
            ++stats_.schedule;
            return;
        }
        pass_on(ev);
    }

private:
    int from_, to_;
    Stats& stats_;
};

class ConfidenceFilter final : public EventFilter {
public:
    ConfidenceFilter(float min, Stats& s) : min_(min), stats_(s) {}
    void handle(const DetectionEvent& ev) override {
        if (ev.confidence < min_) {
            ++stats_.confidence;
            return;
        }
        pass_on(ev);
    }

private:
    float min_;
    Stats& stats_;
};

// Stateful handler: remembers the last alert per (camera, label).
class CooldownFilter final : public EventFilter {
public:
    CooldownFilter(gw::TimestampMs window, Stats& s) : window_(window), stats_(s) {}
    void handle(const DetectionEvent& ev) override {
        const auto key = std::make_pair(ev.camera, ev.label);
        if (auto it = last_.find(key); it != last_.end() && ev.ts - it->second < window_) {
            ++stats_.cooldown;
            return;
        }
        last_[key] = ev.ts;
        pass_on(ev);
    }

private:
    gw::TimestampMs window_;
    Stats& stats_;
    std::map<std::pair<gw::CameraId, std::string>, gw::TimestampMs> last_;
};

class Dispatcher final : public EventFilter {
public:
    explicit Dispatcher(Stats& s) : stats_(s) {}
    void handle(const DetectionEvent&) override { ++stats_.dispatched; }  // publishes the alert

private:
    Stats& stats_;
};

// ---- Client: one chain per camera profile --------------------------------------
std::unique_ptr<EventFilter> outdoor_chain(Stats& s) {
    auto head = std::make_unique<ZoneMaskFilter>(kMaskedZones, s);
    head->then(std::make_unique<ScheduleFilter>(kArmedFrom, kArmedTo, s))
        .then(std::make_unique<ConfidenceFilter>(kMinConfidence, s))
        .then(std::make_unique<CooldownFilter>(kCooldown, s))
        .then(std::make_unique<Dispatcher>(s));
    return head;
}

std::unique_ptr<EventFilter> indoor_chain(Stats& s) {  // same handlers, no schedule
    auto head = std::make_unique<ZoneMaskFilter>(kMaskedZones, s);
    head->then(std::make_unique<ConfidenceFilter>(kMinConfidence, s))
        .then(std::make_unique<CooldownFilter>(kCooldown, s))
        .then(std::make_unique<Dispatcher>(s));
    return head;
}

int main() {
    Stats stats;
    auto outdoor = outdoor_chain(stats);
    for (const auto& ev : night_events()) outdoor->handle(ev);
    CHECK(same(stats, kExpected));  // same numbers as naive.cpp

    Stats indoor_stats;
    auto indoor = indoor_chain(indoor_stats);
    indoor->handle(kIndoorMidday);
    CHECK_EQ(indoor_stats.dispatched, 1);

    // A single handler is testable on its own.
    Stats unit;
    ConfidenceFilter conf(kMinConfidence, unit);
    conf.handle({"cam-x", "car", 0.1f, "any", 0});
    CHECK_EQ(unit.confidence, 1);

    std::cout << "13-chain-of-responsibility/pattern: dispatched=" << stats.dispatched
              << " zone=" << stats.zone << " schedule=" << stats.schedule
              << " confidence=" << stats.confidence << " cooldown=" << stats.cooldown << "\n";
}
