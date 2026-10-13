#pragma once
// Wire formats each transport must produce, shared by naive.cpp, pattern.cpp
// and modern.cpp so all three are checked against the same bytes.
//
//   MQTT    : "alerts/<severity> <title>: <body>"
//   Webhook : {"severity":"<severity>","title":"<title>","body":"<body>"}
//   SMS     : "<SEVERITY> <title>: <body>" cut to 32 characters
//
// Scenario: three alerts (intrusion at "entrance", tamper on cam-2, cam-3
// offline for 15 min), each sent over all three transports → 9 messages.

#include <cstddef>
#include <string>

inline constexpr std::size_t kSmsMax = 32;
inline constexpr int kPerTransport = 3;

inline const std::string kMqttIntrusion = "alerts/high Intrusion: zone=entrance";
inline const std::string kWebhookTamper =
    R"({"severity":"high","title":"Tamper","body":"camera=cam-2 view blocked"})";
inline const std::string kSmsOffline = "LOW Camera offline: camera=cam-3";  // truncated
