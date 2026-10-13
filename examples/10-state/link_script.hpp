#pragma once
// Scripted life of one camera link: connect, stream, lose the stream, fail to
// reconnect with growing backoff, recover, lose it again, give up (Offline),
// operator stops. naive.cpp, pattern.cpp and modern.cpp replay the same script
// and must agree on every number and on the exact sequence of states.

#include <algorithm>
#include <string_view>
#include <vector>

#include "domain.hpp"

// Reconnect policy shared by all three versions.
inline constexpr int kMaxFailures = 3;  // consecutive failed attempts → Offline

constexpr gw::TimestampMs backoff(int failures) {  // 1 s, 2 s, 4 s … capped at 8 s
    return std::min<gw::TimestampMs>(1000LL << failures, 8000);
}

enum class Ev { Start, ConnectOk, ConnectFailed, StreamLost, Frame, Tick, Stop };

struct Event {
    Ev ev;
    gw::TimestampMs ts;
};

inline std::vector<Event> script() {
    return {
        {Ev::Start, 0},            {Ev::ConnectOk, 100},       {Ev::Frame, 200},
        {Ev::Frame, 300},          {Ev::Frame, 400},           {Ev::StreamLost, 500},   // retry at 1500
        {Ev::Frame, 600},          {Ev::Tick, 1000},           {Ev::Tick, 1500},        // 1000: too early
        {Ev::ConnectFailed, 1600}, {Ev::Tick, 3000},           {Ev::Tick, 3600},        // retry at 3600
        {Ev::ConnectFailed, 3700}, {Ev::Tick, 7700},           {Ev::ConnectOk, 7800},   // retry at 7700
        {Ev::Frame, 7900},         {Ev::Frame, 7950},          {Ev::StreamLost, 8000},
        {Ev::Tick, 9000},          {Ev::ConnectFailed, 9100},  {Ev::Tick, 11100},
        {Ev::ConnectFailed, 11200},{Ev::Tick, 15200},          {Ev::ConnectFailed, 15300},  // 3rd → Offline
        {Ev::Frame, 15400},        {Ev::Tick, 20000},          {Ev::Stop, 21000},
    };
}

// Feeds the script to any link with the same public API. The switch here only
// decodes the script format; it is not the state machine.
template <class Link>
void replay(Link& link) {
    for (const auto& [ev, ts] : script()) {
        switch (ev) {
            case Ev::Start:         link.start(); break;
            case Ev::ConnectOk:     link.connect_ok(); break;
            case Ev::ConnectFailed: link.connect_failed(ts); break;
            case Ev::StreamLost:    link.stream_lost(ts); break;
            case Ev::Frame:         link.frame(); break;
            case Ev::Tick:          link.tick(ts); break;
            case Ev::Stop:          link.stop(); break;
        }
    }
}

inline constexpr int kForwarded = 5;        // frames while Streaming
inline constexpr int kDropped = 2;          // frames in Reconnecting and Offline
inline constexpr int kConnectAttempts = 7;  // entries into Connecting
inline constexpr std::string_view kTrace =
    "Disconnected>Connecting>Streaming>Reconnecting>Connecting>Reconnecting>Connecting"
    ">Reconnecting>Connecting>Streaming>Reconnecting>Connecting>Reconnecting>Connecting"
    ">Reconnecting>Connecting>Offline>Disconnected";
