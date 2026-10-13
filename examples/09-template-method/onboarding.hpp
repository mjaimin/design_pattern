#pragma once
// Chapter 09 shared data: stream profiles the two (simulated) vendors report,
// and the onboarding log each path must produce.
//
// Onboarding sequence, identical for every vendor:
//   probe → authenticate → fetch profiles → select profile → [hook] → configure → verify
// Vendor-specific: probe, authenticate, fetch, configure, and the optional hook.
// Vendor-independent: the order, the selection rule, the verification.

#include <format>
#include <string>
#include <vector>

struct Profile {
    int width;
    int height;
    int fps;
};

inline std::string label(const Profile& p) { return std::format("{}x{}@{}", p.width, p.height, p.fps); }

// Selection rule (gateway policy, not vendor knowledge):
// largest resolution whose height is ≤ kMaxHeight.
inline constexpr int kMaxHeight = 1080;

inline const std::vector<Profile> kVendorAProfiles = {{3840, 2160, 15}, {1920, 1080, 15}, {640, 360, 15}};
inline const std::vector<Profile> kVendorBProfiles = {{2560, 1440, 20}, {1280, 720, 25}, {640, 480, 25}};

// ---- Expected results ---------------------------------------------------------
inline const std::vector<std::string> kVendorALog = {
    "A: GET /api/device-info", "A: basic auth", "A: list 3 profiles",
    "select 1920x1080@15",     "A: PUT stream 1920x1080@15", "verify stream 1920x1080@15",
};
inline const std::vector<std::string> kVendorBLog = {
    "B: GetDeviceInformation", "B: digest auth",           "B: list 3 profiles",
    "select 1280x720@25",      "B: disable smart codec",   "B: SetEncoder 1280x720@25",
    "verify stream 1280x720@25",
};
