#pragma once
// Simulated detector output for the overlay renderer: 500 frames × 20 boxes
// = 10 000 bounding boxes, drawn from 4 object classes. Shared by naive.cpp,
// pattern.cpp and modern.cpp so all three must render identical results.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

inline constexpr int kFrames = 500;
inline constexpr int kBoxesPerFrame = 20;
inline constexpr int kTotalBoxes = kFrames * kBoxesPerFrame;

// Per-class metadata as the model/config defines it. This is the data that
// is identical for every "person" box ever drawn.
struct ClassSpec {
    std::string_view label;
    std::string_view display_name;
    std::uint32_t rgba;
    int priority;  // draw order / alert weight
};

inline constexpr std::array<ClassSpec, 4> kClassSpecs = {{
    {"person", "Person", 0xFF3B30FF, 3},
    {"vehicle", "Vehicle", 0x007AFFFF, 2},
    {"bicycle", "Bicycle", 0x34C759FF, 2},
    {"animal", "Animal", 0xFF9500FF, 1},
}};

// Stand-in for the class icon the UI overlays next to each box: ~1 KiB of SVG.
inline std::string make_icon_svg(std::string_view label) {
    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'><path d='";
    while (svg.size() < 1000) svg.append("M4 4h16v16H4z ");
    svg.append("'/><title>").append(label).append("</title></svg>");
    return svg;
}

// What the detector hands over per box: position + which class (by index).
struct RawDetection {
    int class_index;
    int x, y, w, h;
    float confidence;
    int track_id;
};

inline std::vector<RawDetection> detector_output() {
    std::vector<RawDetection> out;
    out.reserve(kTotalBoxes);
    std::uint32_t seed = 12345;
    auto next = [&seed] { return seed = seed * 1664525u + 1013904223u; };  // LCG: deterministic
    for (int i = 0; i < kTotalBoxes; ++i) {
        const auto r = next();
        out.push_back({static_cast<int>(r >> 30), static_cast<int>(r >> 8) % 1920,
                       static_cast<int>(r >> 16) % 1080, 40 + static_cast<int>(r % 200),
                       80 + static_cast<int>(r % 300), 0.5f + static_cast<float>(r % 50) / 100.0f,
                       i / kBoxesPerFrame * 7 + i % kBoxesPerFrame});
    }
    return out;
}

// What "rendering" produces, reduced to numbers the examples can compare.
struct RenderStats {
    int boxes = 0;
    std::array<int, 4> per_class{};
    std::uint64_t checksum = 0;  // folds colour, position, label and icon size of every box
};

inline void fold(RenderStats& s, int class_index, std::uint32_t rgba, int x, int y,
                 std::size_t label_len, std::size_t icon_len) {
    ++s.boxes;
    ++s.per_class[static_cast<std::size_t>(class_index)];
    s.checksum = s.checksum * 31 + rgba + static_cast<std::uint64_t>(x) * 7 +
                 static_cast<std::uint64_t>(y) + label_len + icon_len;
}

// Expected render result for detector_output() (same in naive, pattern, modern).
inline constexpr std::array<int, 4> kExpectedPerClass = {2555, 2392, 2600, 2453};
inline constexpr std::uint64_t kExpectedChecksum = 8527818898783396641u;
