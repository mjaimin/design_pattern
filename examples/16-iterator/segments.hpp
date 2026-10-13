#pragma once
// Recorded video is stored as fixed-length segments in one directory per
// day:  /recordings/2026-10-11/cam-1_080000.mp4, ...
// In memory that layout is a map: day → segments recorded that day.
// Shared by naive.cpp, pattern.cpp and modern.cpp.

#include <map>
#include <string>
#include <vector>

#include "domain.hpp"

struct Segment {
    gw::CameraId camera;
    gw::TimestampMs start;
    gw::TimestampMs duration;
};

using DayIndex = std::map<std::string, std::vector<Segment>>;  // ordered by date

// Three days; the middle one is empty (disk was offline).
inline DayIndex sample_index() {
    return {
        {"2026-10-11",
         {{"cam-1", gw::at(8, 0), 60'000},
          {"cam-2", gw::at(8, 0), 60'000},
          {"cam-1", gw::at(8, 1), 60'000},
          {"cam-1", gw::at(8, 2), 30'000},
          {"cam-2", gw::at(9, 0), 60'000}}},
        {"2026-10-12", {}},
        {"2026-10-13",
         {{"cam-1", gw::at(22, 0), 60'000},
          {"cam-2", gw::at(22, 0), 60'000},
          {"cam-1", gw::at(22, 1), 45'000}}},
    };
}

// Expected results over sample_index():
inline constexpr int kTotalSegments = 8;
inline constexpr int kCam1Segments = 5;
inline constexpr gw::TimestampMs kCam1Duration = 60'000 + 60'000 + 30'000 + 60'000 + 45'000;  // 255 s
