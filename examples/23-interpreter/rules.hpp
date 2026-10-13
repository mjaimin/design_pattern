#pragma once
// Shared by naive.cpp, pattern.cpp and modern.cpp: the event type rules are
// evaluated against, a simulated day of events, the tokenizer, and small
// parsing helpers for the leaf terms.
//
// The rule language (what an operator types into the gateway UI):
//
//   rule    := or
//   or      := and ( "OR" and )*
//   and     := unary ( "AND" unary )*
//   unary   := "NOT" unary | "(" or ")" | term
//   term    := LABEL                     e.g.  person
//            | "zone=" NAME              e.g.  zone=entrance
//            | "confidence>=" NUMBER     e.g.  confidence>=0.8
//            | "time" HH:MM-HH:MM        e.g.  time 22:00-06:00   (wraps midnight)
//
// Precedence: NOT > AND > OR, as in C and SQL.

#include <array>
#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "domain.hpp"

struct Event {
    std::string label;
    std::string zone;
    gw::TimestampMs ts_ms;
    double confidence;
};

// One event every 5 minutes for 24 h = 288 events.
inline std::vector<Event> simulated_events() {
    static constexpr std::array<std::string_view, 5> labels = {"person", "vehicle", "person", "animal", "bicycle"};
    static constexpr std::array<std::string_view, 3> zones = {"entrance", "parking", "perimeter"};
    std::vector<Event> out;
    for (int i = 0; i < 288; ++i)
        out.push_back({std::string(labels[static_cast<std::size_t>(i % 5)]),
                       std::string(zones[static_cast<std::size_t>(i % 3)]), gw::at(0, i * 5),
                       (40 + (i % 7) * 10) / 100.0});  // 0.4 … 1.0; same double as parsing "0.8"
    return out;
}

// --- Rules every implementation must agree on (AND-only: the naive version's limit)
inline constexpr std::string_view kNightEntrance = "person AND zone=entrance AND time 22:00-06:00";
inline constexpr std::string_view kParkingVehicle = "vehicle AND zone=parking";
inline constexpr std::string_view kConfidentDaytime = "confidence>=0.8 AND time 08:00-18:00";
inline constexpr int kNightEntranceHits = 13;
inline constexpr int kParkingVehicleHits = 20;
inline constexpr int kConfidentDaytimeHits = 52;

// --- Rules only a real grammar can express
inline constexpr std::string_view kPeopleOrCarsOutsideParking = "(person OR vehicle) AND NOT zone=parking";
inline constexpr std::string_view kPrecedence = "animal OR bicycle AND confidence>=0.9";
inline constexpr int kPeopleOrCarsOutsideParkingHits = 116;
inline constexpr int kPrecedenceHits = 73;

// --- Tokenizer: whitespace-separated words; parentheses are their own tokens.
inline std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::string cur;
    auto flush = [&] {
        if (!cur.empty()) tokens.push_back(std::move(cur)), cur.clear();
    };
    for (char c : text) {
        if (c == ' ' || c == '\t') {
            flush();
        } else if (c == '(' || c == ')') {
            flush();
            tokens.emplace_back(1, c);
        } else {
            cur += c;
        }
    }
    flush();
    return tokens;
}

// --- Leaf helpers ---------------------------------------------------------------
inline int minute_of_day(gw::TimestampMs ts) {
    return static_cast<int>((ts / gw::at(0, 1)) % (24 * 60));
}

// "22:00-06:00" → {1320, 360}. Throws on malformed input.
struct Window {
    int from;
    int to;
};
inline Window parse_window(std::string_view s) {
    auto hhmm = [&](std::string_view part) {
        int h = -1, m = -1;
        const auto r1 = std::from_chars(part.data(), part.data() + 2, h);
        const auto r2 = std::from_chars(part.data() + 3, part.data() + 5, m);
        if (part.size() != 5 || part[2] != ':' || r1.ec != std::errc{} || r2.ec != std::errc{} ||
            h < 0 || h > 23 || m < 0 || m > 59)
            throw std::invalid_argument("bad time: " + std::string(s));
        return h * 60 + m;
    };
    if (s.size() != 11 || s[5] != '-') throw std::invalid_argument("bad time window: " + std::string(s));
    return {hhmm(s.substr(0, 5)), hhmm(s.substr(6, 5))};
}

// [from, to) in minutes of day; from > to means the window wraps midnight.
inline bool in_window(Window w, gw::TimestampMs ts) {
    const int m = minute_of_day(ts);
    return w.from <= w.to ? (m >= w.from && m < w.to) : (m >= w.from || m < w.to);
}

inline double parse_number(std::string_view s) {
    double v = 0;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    if (r.ec != std::errc{} || r.ptr != s.data() + s.size())
        throw std::invalid_argument("bad number: " + std::string(s));
    return v;
}
