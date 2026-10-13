#pragma once
// Chapter 08 receivers: the devices that actually do the work.
// Shared by naive.cpp, pattern.cpp and modern.cpp.
//
// Scenario used by all three files:
//   intrusion alarm  → siren on, floodlight on, PTZ camera to preset 3 (entrance)
//   operator ack     → undo all three, newest first
// The siren's relay board drops the first command it receives, so every path
// must retry (max 3 attempts) to get the same result.

#include <string>
#include <vector>

class Relay {
public:
    explicit Relay(std::string name, int drop_first_n = 0) : name_(std::move(name)), drop_(drop_first_n) {}

    bool set(bool on) {
        ++attempts_;
        if (drop_ > 0) {  // simulated: no ACK from the relay board
            --drop_;
            return false;
        }
        on_ = on;
        return true;
    }

    bool is_on() const { return on_; }
    int attempts() const { return attempts_; }
    const std::string& name() const { return name_; }

private:
    std::string name_;
    int drop_;
    bool on_ = false;
    int attempts_ = 0;
};

class PtzCamera {
public:
    explicit PtzCamera(std::string name, int preset = 1) : name_(std::move(name)), preset_(preset) {}

    bool goto_preset(int preset) {
        preset_ = preset;
        return true;
    }

    int preset() const { return preset_; }
    const std::string& name() const { return name_; }

private:
    std::string name_;
    int preset_;
};

inline constexpr int kMaxAttempts = 3;

// ---- Expected results ---------------------------------------------------------
inline const std::vector<std::string> kAuditLog = {
    "siren: on", "floodlight: on", "ptz-1: preset 3",     // alarm
    "ptz-1: preset 1", "floodlight: off", "siren: off",   // acknowledge (reverse order)
};
inline constexpr int kSirenAttempts = 3;  // 1 dropped + 1 ok (alarm), 1 ok (undo)
inline constexpr int kFloodAttempts = 2;
