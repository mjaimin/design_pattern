#pragma once
// Actuators shared by naive.cpp and pattern.cpp. They take commands and know
// nothing about any other device — that part never changes. What differs
// between the two files is *who decides* which actuator to drive and when.

#include <string>

#include "domain.hpp"

class Siren {
public:
    void on() {
        if (!active_) {
            active_ = true;
            ++activations_;
        }
    }
    void off() { active_ = false; }
    bool active() const { return active_; }
    int activations() const { return activations_; }

private:
    bool active_ = false;
    int activations_ = 0;
};

class Camera {
public:
    explicit Camera(gw::CameraId id) : id_(std::move(id)) {}
    void start_recording() {
        if (!recording_) {
            recording_ = true;
            ++clips_;
        }
    }
    void stop_recording() { recording_ = false; }
    bool recording() const { return recording_; }
    int clips() const { return clips_; }

private:
    gw::CameraId id_;
    bool recording_ = false;
    int clips_ = 0;
};

class DoorLock {
public:
    void lock() {
        if (!locked_) {
            locked_ = true;
            ++toggles_;
        }
    }
    void unlock() {
        if (locked_) {
            locked_ = false;
            ++toggles_;
        }
    }
    bool locked() const { return locked_; }
    int toggles() const { return toggles_; }

private:
    bool locked_ = false;
    int toggles_ = 0;
};

inline constexpr int kArmCode = 1234;

// Outcome of the shared scenario (see main() in naive.cpp / pattern.cpp):
//   open (disarmed) → arm → forced open → open again → wrong code → disarm → open
inline constexpr int kSirenActivations = 1;  // second opening while sounding is not a new alarm
inline constexpr int kClipsPerCamera = 1;
inline constexpr int kLockToggles = 2;       // locked on arm, unlocked on disarm
