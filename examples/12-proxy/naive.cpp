// 12 Proxy — the naive version.
//
// One Camera class does everything: talks to the device, connects lazily,
// caches snapshots, and checks the caller's role before PTZ.
//
// Where it hurts:
//   * The device driver knows about user roles. `Role` leaks into the
//     signature of every protected method, and into every caller.
//   * Caching is welded to this driver. The next camera driver (another
//     vendor) re-implements the same TTL logic, slightly differently.
//   * A unit test of "does the snapshot request have the right URL?" has to
//     dodge the cache; a test of the cache needs a whole camera.
//   * The connect-on-first-use check is repeated at the top of every method.
// Smells: Divergent Change (the class changes for protocol, cache policy and
// access-control reasons) and Duplicate Code across drivers.

#include <optional>
#include <string>

#include "check.hpp"
#include "fixture.hpp"

class Camera {
public:
    Camera(std::string host, Stats& stats, gw::TimestampMs cache_ttl)
        : host_(std::move(host)), stats_(stats), ttl_(cache_ttl) {}

    std::string snapshot(gw::TimestampMs now) {
        // caching concern
        if (cached_ && now - cached_at_ < ttl_) return *cached_;
        // lazy-connect concern
        ensure_connected();
        // the actual job
        ++stats_.fetches;
        cached_ = jpeg_at(now);
        cached_at_ = now;
        return *cached_;
    }

    bool ptz_move(Role role, int /*pan*/, int /*tilt*/) {
        // access-control concern
        if (role < Role::Operator) {
            ++stats_.denied;
            return false;
        }
        // lazy-connect concern, again
        ensure_connected();
        // the actual job
        ++stats_.moves;
        return true;
    }

private:
    void ensure_connected() {
        if (connected_) return;
        ++stats_.connects;  // TCP + RTSP DESCRIBE + auth
        connected_ = true;
    }

    std::string host_;
    Stats& stats_;
    bool connected_ = false;
    gw::TimestampMs ttl_;
    std::optional<std::string> cached_;
    gw::TimestampMs cached_at_ = 0;
};

int main() {
    Stats stats;
    Camera cam1("10.0.0.11", stats, kSnapshotTtl);
    Camera cam2("10.0.0.12", stats, kSnapshotTtl);  // created at start, never used

    CHECK(cam1.snapshot(0) == jpeg_at(0));
    CHECK(cam1.snapshot(500) == jpeg_at(0));  // cached
    CHECK(cam1.snapshot(1500) == jpeg_at(1500));
    CHECK(!cam1.ptz_move(Role::Viewer, 10, 0));
    CHECK(cam1.ptz_move(Role::Admin, 10, 0));

    CHECK_EQ(stats.connects, kConnects);
    CHECK_EQ(stats.fetches, kFetches);
    CHECK_EQ(stats.moves, kMoves);
    CHECK_EQ(stats.denied, kDenied);

    std::cout << "12-proxy/naive: connects=" << kConnects << " fetches=" << kFetches
              << " moves=" << kMoves << " denied=" << kDenied << "\n";
}
