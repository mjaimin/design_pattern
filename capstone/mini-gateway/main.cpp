// Capstone — a mini gateway built from the patterns in this repo.
// See README.md in this directory for the pattern map and the places where
// a pattern was deliberately NOT used.
//
// Scenario: two cameras from two vendors, 60 s at 1 frame/s.
//   cam-A (vendor A SDK)  motion at 10, 11, 12, 40 s
//   cam-B (vendor B HTTP) motion at 20, 21, 22 s
// The simulated AI detector reports, on motion frames:
//   A@10 person 0.92 entrance   → passes, relay pulse
//   A@11 person 0.95 entrance   → dropped by Cooldown (1 s after A@10)
//   A@12 car    0.40 driveway   → dropped by MinConfidence
//   A@40 person 0.90 entrance   → passes (30 s after A@10), relay pulse
//   B@20 person 0.88 parking    → passes, no relay (not the alarm zone)
//   B@21 person 0.91 street     → dropped by ZoneMask
//   B@22 car    0.85 parking    → passes

#include <map>
#include <tuple>

#include "check.hpp"
#include "gateway.hpp"

namespace {

gw::TimestampMs sec(int s) { return gw::at(0, 0, s); }

std::unique_ptr<mg::Camera> make_cam_a() {
    std::vector<vendor_a::RawFrame> script;
    for (int s = 0; s < 60; ++s) {
        const bool motion = s == 10 || s == 11 || s == 12 || s == 40;
        script.push_back({s * 1'000'000LL, static_cast<std::uint8_t>(motion)});
    }
    return std::make_unique<mg::VendorACamera>("cam-A", vendor_a::Device(std::move(script)));
}

std::unique_ptr<mg::Camera> make_cam_b() {
    std::vector<std::string> responses;
    for (int s = 0; s < 60; ++s) {
        const bool motion = s >= 20 && s <= 22;
        responses.push_back("ts_ms=" + std::to_string(sec(s)) + ";motion=" + (motion ? "1" : "0"));
    }
    return std::make_unique<mg::VendorBCamera>("cam-B", vendor_b::Client(std::move(responses)));
}

// Stand-in for an inference engine: a lookup of what "the model" sees.
mg::Detector make_detector() {
    using Key = std::pair<gw::CameraId, gw::TimestampMs>;
    using Hit = std::tuple<std::string, float, std::string>;
    std::map<Key, Hit> hits = {
        {{"cam-A", sec(10)}, {"person", 0.92f, "entrance"}},
        {{"cam-A", sec(11)}, {"person", 0.95f, "entrance"}},
        {{"cam-A", sec(12)}, {"car", 0.40f, "driveway"}},
        {{"cam-A", sec(40)}, {"person", 0.90f, "entrance"}},
        {{"cam-B", sec(20)}, {"person", 0.88f, "parking"}},
        {{"cam-B", sec(21)}, {"person", 0.91f, "street"}},
        {{"cam-B", sec(22)}, {"car", 0.85f, "parking"}},
    };
    return [hits = std::move(hits)](const gw::Frame& f) -> std::optional<mg::DetectionEvent> {
        if (!f.motion) return std::nullopt;
        auto it = hits.find({f.camera, f.ts_ms});
        if (it == hits.end()) return std::nullopt;
        const auto& [label, conf, zone] = it->second;
        return mg::DetectionEvent{f.camera, label, conf, zone, f.ts_ms};
    };
}

}  // namespace

int main() {
    mg::Gateway gateway({.masked_zones = {"street"}}, make_detector());
    gateway.add_camera(make_cam_a());
    gateway.add_camera(make_cam_b());

    // An outside subscriber — e.g. the live UI — attached through the facade.
    std::vector<mg::DetectionEvent> seen;
    auto ui = gateway.subscribe([&](const mg::DetectionEvent& e) { seen.push_back(e); });

    // A subscriber that goes out of scope before run(): its RAII token
    // unsubscribes it, so it is never called with a dangling capture.
    int short_lived_calls = 0;
    { auto tmp = gateway.subscribe([&](const mg::DetectionEvent&) { ++short_lived_calls; }); }

    gateway.run();

    // Filter chain + observer: 4 of 7 detections survive, in time order.
    CHECK_EQ(seen.size(), 4);
    CHECK(seen[0].camera == "cam-A" && seen[0].ts_ms == sec(10));
    CHECK(seen[1].camera == "cam-B" && seen[1].label == "person");
    CHECK(seen[2].camera == "cam-B" && seen[2].label == "car");
    CHECK(seen[3].camera == "cam-A" && seen[3].ts_ms == sec(40));
    CHECK_EQ(short_lived_calls, 0);

    // Command: one relay pulse per person in the entrance zone.
    CHECK_EQ(gateway.relay_activations(), 2);
    CHECK_EQ(gateway.action_log().size(), 2);
    CHECK_EQ(gateway.action_log()[0], std::string("pulse relay 1 for 5s (cam-A person at entrance)"));

    // Strategy (motion-triggered, 2 s post-roll), one policy per camera:
    //   cam-A: 10-14 (5) + 40-42 (3) = 8     cam-B: 20-24 (5)
    CHECK_EQ(gateway.frames_recorded("cam-A"), 8);
    CHECK_EQ(gateway.frames_recorded("cam-B"), 5);

    std::cout << "capstone/mini-gateway: events=" << seen.size()
              << " relay_pulses=" << gateway.relay_activations()
              << " recorded A=" << gateway.frames_recorded("cam-A")
              << " B=" << gateway.frames_recorded("cam-B") << "\n";
    for (const auto& line : gateway.action_log()) std::cout << "  " << line << "\n";
}
