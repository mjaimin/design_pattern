#pragma once
// Parts of a camera's configuration, shared by every file in this chapter.
// They are chosen to show the three copy behaviours a clone must get right:
//   Zone       — plain value, copied by the member-wise copy for free
//   MotionGrid — owned through unique_ptr: must be *deep*-copied by hand
//   OsdFont    — immutable and large: deliberately *shared* (shared_ptr<const>)

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct Zone {
    std::string name;
    std::vector<std::pair<int, int>> polygon;  // privacy mask / detection area
};

struct MotionGrid {
    std::array<bool, 16 * 9> cells{};
    int sensitivity = 50;
};

struct OsdFont {
    std::string family;
    std::vector<unsigned char> glyphs;  // imagine a few hundred KB of bitmap
};

// Scenario: onboard 50 lobby domes and 10 parking bullets from two templates.
inline constexpr int kDomes = 50;
inline constexpr int kBullets = 10;
inline constexpr int kTemplateSensitivity = 70;
inline constexpr int kZonesPerTemplate = 2;
