#pragma once
// The site used by every file in this chapter:
//
//   gw-1 (online)                      gw-2 (online)
//   ├── camera cam-a  online, AI       ├── camera cam-c  online
//   ├── camera cam-b  OFFLINE          └── relay  r2     online
//   ├── relay  r1     online
//   └── sensor s1     OFFLINE
//
// Three operations run over it; the numbers must match in every file.

#include <string>
#include <vector>

// Licences: camera = 1, AI-enabled camera = 2, everything else = 0.
inline constexpr int kLicences = 2 + 1 + 1;

inline const std::vector<std::string> kOffline = {"cam-b", "s1"};

inline const std::string kCsv =
    "gateway,gw-1,online\n"
    "camera,cam-a,online\n"
    "camera,cam-b,offline\n"
    "relay,r1,online\n"
    "sensor,s1,offline\n"
    "gateway,gw-2,online\n"
    "camera,cam-c,online\n"
    "relay,r2,online\n";
