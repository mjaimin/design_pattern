#pragma once
// Shared by naive.cpp, pattern.cpp and modern.cpp: who is asking, what the
// expensive operations cost, and the expected counts for one scripted session.
//
// Script:
//   two camera handles are created at gateway start; only cam-1 is ever used
//   viewer   snapshot @ t=0      → connect + fetch
//   viewer   snapshot @ t=500    → served from cache (TTL 1 s)
//   viewer   snapshot @ t=1500   → cache expired → fetch
//   viewer   ptz_move            → denied (needs Operator or higher)
//   admin    ptz_move            → moves

#include <string>

#include "domain.hpp"

enum class Role { Viewer, Operator, Admin };

// What the gateway pays for. Each counter is one network round trip (or a
// TCP+RTSP+auth handshake, for connects) against a real camera.
struct Stats {
    int connects = 0;
    int fetches = 0;
    int moves = 0;
    int denied = 0;
};

inline constexpr gw::TimestampMs kSnapshotTtl = 1000;

inline std::string jpeg_at(gw::TimestampMs ts) { return "jpeg@" + std::to_string(ts); }

inline constexpr int kConnects = 1;  // cam-2 never connects
inline constexpr int kFetches = 2;
inline constexpr int kMoves = 1;
inline constexpr int kDenied = 1;
