#pragma once
// The three hardware-facing parts every gateway needs, and their concrete
// versions for two platforms. Shared by naive.cpp and pattern.cpp: the
// *products* are the same in both — what differs is how they get created.
//
//                 Decoder          RelayOutput        Watchdog
//   ARM appliance HwDecoder        GpioRelay          HwWatchdog
//   x86 VM        SwDecoder        SimulatedRelay     SystemdWatchdog
//
// A row is a *family*: the parts only work together. HwDecoder needs the
// appliance's video codec block; GpioRelay needs its GPIO pins; on a VM
// neither exists.

#include <string>

enum class Platform { ArmAppliance, X86Vm };

// ---- Abstract products -----------------------------------------------------
class Decoder {
public:
    virtual ~Decoder() = default;
    virtual int max_streams() const = 0;
    virtual std::string name() const = 0;
    virtual Platform platform() const = 0;
};

class RelayOutput {
public:
    virtual ~RelayOutput() = default;
    virtual void pulse() = 0;
    virtual int pulses() const = 0;
    virtual std::string name() const = 0;
    virtual Platform platform() const = 0;
};

class Watchdog {
public:
    virtual ~Watchdog() = default;
    virtual void kick() = 0;
    virtual int kicks() const = 0;
    virtual std::string name() const = 0;
    virtual Platform platform() const = 0;
};

// ---- ARM appliance family --------------------------------------------------
class HwDecoder final : public Decoder {
public:
    int max_streams() const override { return 16; }  // hardware codec block
    std::string name() const override { return "hw-decoder"; }
    Platform platform() const override { return Platform::ArmAppliance; }
};

class GpioRelay final : public RelayOutput {
public:
    void pulse() override { ++pulses_; }  // stands in for pin high → sleep → pin low
    int pulses() const override { return pulses_; }
    std::string name() const override { return "gpio-relay"; }
    Platform platform() const override { return Platform::ArmAppliance; }

private:
    int pulses_ = 0;
};

class HwWatchdog final : public Watchdog {
public:
    void kick() override { ++kicks_; }  // stands in for write to /dev/watchdog
    int kicks() const override { return kicks_; }
    std::string name() const override { return "hw-watchdog"; }
    Platform platform() const override { return Platform::ArmAppliance; }

private:
    int kicks_ = 0;
};

// ---- x86 VM family ---------------------------------------------------------
class SwDecoder final : public Decoder {
public:
    int max_streams() const override { return 4; }  // CPU-bound
    std::string name() const override { return "sw-decoder"; }
    Platform platform() const override { return Platform::X86Vm; }
};

class SimulatedRelay final : public RelayOutput {
public:
    void pulse() override { ++pulses_; }  // stands in for a log line / test hook
    int pulses() const override { return pulses_; }
    std::string name() const override { return "sim-relay"; }
    Platform platform() const override { return Platform::X86Vm; }

private:
    int pulses_ = 0;
};

class SystemdWatchdog final : public Watchdog {
public:
    void kick() override { ++kicks_; }  // stands in for sd_notify("WATCHDOG=1")
    int kicks() const override { return kicks_; }
    std::string name() const override { return "systemd-watchdog"; }
    Platform platform() const override { return Platform::X86Vm; }

private:
    int kicks_ = 0;
};

// Expected results of the boot scenario (request 10 streams, pulse 2, kick 3):
inline constexpr int kRequestedStreams = 10;
inline constexpr int kArmAccepted = 10;  // min(10, 16)
inline constexpr int kVmAccepted = 4;    // min(10, 4)
inline constexpr int kPulses = 2;
inline constexpr int kKicks = 3;
