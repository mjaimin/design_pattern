// 12 Proxy — classic form: three proxies over one Camera interface.
//
// Force: *access* to a camera must be controlled — deferred until first use,
//        short-circuited by a cache, refused for the wrong role — while the
//        driver and every caller keep talking to the same Camera interface.
//
// Roles
//   Subject      : Camera         — the interface callers use
//   Real subject : RealCamera     — talks to the device; knows nothing else
//   Proxies      : LazyCamera     — virtual proxy: creates the real one on first use
//                  CachingCamera  — caching proxy: serves recent snapshots
//                  GuardedCamera  — protection proxy: checks the session's role
//
// Each proxy has the same interface as the subject and *decides whether and
// when* the request reaches it. That decision is what makes it a proxy rather
// than a decorator.

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "check.hpp"
#include "fixture.hpp"

// ---- Subject -------------------------------------------------------------------
class Camera {
public:
    virtual ~Camera() = default;
    virtual std::string snapshot(gw::TimestampMs now) = 0;
    virtual bool ptz_move(int pan, int tilt) = 0;
};

// ---- Real subject: only the device protocol ----------------------------------
class RealCamera final : public Camera {
public:
    RealCamera(std::string host, Stats& stats) : host_(std::move(host)), stats_(stats) {
        ++stats_.connects;  // TCP + RTSP DESCRIBE + auth happen in the constructor
    }
    std::string snapshot(gw::TimestampMs now) override {
        ++stats_.fetches;
        return jpeg_at(now);
    }
    bool ptz_move(int, int) override {
        ++stats_.moves;
        return true;
    }

private:
    std::string host_;
    Stats& stats_;
};

// ---- Virtual proxy: create the expensive object on first use -----------------
class LazyCamera final : public Camera {
public:
    using Factory = std::function<std::unique_ptr<Camera>()>;
    explicit LazyCamera(Factory make) : make_(std::move(make)) {}

    std::string snapshot(gw::TimestampMs now) override { return real().snapshot(now); }
    bool ptz_move(int pan, int tilt) override { return real().ptz_move(pan, tilt); }

private:
    Camera& real() {
        if (!real_) real_ = make_();
        return *real_;
    }
    Factory make_;
    std::unique_ptr<Camera> real_;  // owns the subject it creates
};

// ---- Caching proxy: answer from cache while fresh -----------------------------
class CachingCamera final : public Camera {
public:
    CachingCamera(Camera& inner, gw::TimestampMs ttl) : inner_(inner), ttl_(ttl) {}

    std::string snapshot(gw::TimestampMs now) override {
        if (!cached_ || now - cached_at_ >= ttl_) {
            cached_ = inner_.snapshot(now);  // request reaches the subject only on a miss
            cached_at_ = now;
        }
        return *cached_;
    }
    bool ptz_move(int pan, int tilt) override { return inner_.ptz_move(pan, tilt); }

private:
    Camera& inner_;
    gw::TimestampMs ttl_;
    std::optional<std::string> cached_;
    gw::TimestampMs cached_at_ = 0;
};

// ---- Protection proxy: one per user session ----------------------------------
class GuardedCamera final : public Camera {
public:
    GuardedCamera(Camera& inner, Role role, Stats& stats) : inner_(inner), role_(role), stats_(stats) {}

    std::string snapshot(gw::TimestampMs now) override { return inner_.snapshot(now); }  // any role
    bool ptz_move(int pan, int tilt) override {
        if (role_ < Role::Operator) {
            ++stats_.denied;
            return false;  // request never reaches the device
        }
        return inner_.ptz_move(pan, tilt);
    }

private:
    Camera& inner_;
    Role role_;
    Stats& stats_;
};

int main() {
    Stats stats;

    // Gateway start: handles for every configured camera, no connections yet.
    LazyCamera cam1([&] { return std::make_unique<RealCamera>("10.0.0.11", stats); });
    LazyCamera cam2([&] { return std::make_unique<RealCamera>("10.0.0.12", stats); });
    CHECK_EQ(stats.connects, 0);

    // One shared cache in front of cam-1; one guard per logged-in session.
    CachingCamera cam1_cached(cam1, kSnapshotTtl);
    GuardedCamera viewer(cam1_cached, Role::Viewer, stats);
    GuardedCamera admin(cam1_cached, Role::Admin, stats);

    // Callers see only Camera&.
    Camera& v = viewer;
    CHECK(v.snapshot(0) == jpeg_at(0));
    CHECK(v.snapshot(500) == jpeg_at(0));  // cached
    CHECK(v.snapshot(1500) == jpeg_at(1500));
    CHECK(!v.ptz_move(10, 0));
    CHECK(admin.ptz_move(10, 0));

    // Same numbers as naive.cpp.
    CHECK_EQ(stats.connects, kConnects);
    CHECK_EQ(stats.fetches, kFetches);
    CHECK_EQ(stats.moves, kMoves);
    CHECK_EQ(stats.denied, kDenied);

    std::cout << "12-proxy/pattern: connects=" << stats.connects << " fetches=" << stats.fetches
              << " moves=" << stats.moves << " denied=" << stats.denied << "\n";
}
