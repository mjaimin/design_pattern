// 14 Abstract Factory — the naive version.
//
// Gateway builds its own parts. Each part is chosen by its own `if` on the
// platform — or, as here, on its own config flag, because the decoder grew a
// "hw_decode" switch at some point and the relay/watchdog didn't.
//
// Where it hurts:
//   1. Nothing ties the three choices together. `hw_decode = true` on a VM
//      builds HwDecoder + SimulatedRelay + SystemdWatchdog — a mix that
//      compiles, boots, and fails at the first decode (no codec block).
//   2. Adding a platform (a test rig, a second appliance model) means finding
//      and editing every `if (platform == ...)` — here three, in a real
//      gateway dozens, spread over many files (Shotgun Surgery).
//   3. Gateway depends on all six concrete classes, so it can't be unit
//      tested without dragging in both platforms' code.

#include <algorithm>
#include <memory>

#include "check.hpp"
#include "platform_parts.hpp"

struct GatewayConfig {
    Platform platform;
    bool hw_decode;  // added later for the decoder only
};

class Gateway {
public:
    explicit Gateway(const GatewayConfig& cfg) {
        // choice #1
        if (cfg.hw_decode)
            decoder_ = std::make_unique<HwDecoder>();
        else
            decoder_ = std::make_unique<SwDecoder>();

        // choice #2
        if (cfg.platform == Platform::ArmAppliance)
            relay_ = std::make_unique<GpioRelay>();
        else
            relay_ = std::make_unique<SimulatedRelay>();

        // choice #3
        if (cfg.platform == Platform::ArmAppliance)
            watchdog_ = std::make_unique<HwWatchdog>();
        else
            watchdog_ = std::make_unique<SystemdWatchdog>();
    }

    int start_streams(int requested) { return std::min(requested, decoder_->max_streams()); }
    void trigger_relay() { relay_->pulse(); }
    void heartbeat() { watchdog_->kick(); }

    std::string signature() const {
        return decoder_->name() + "+" + relay_->name() + "+" + watchdog_->name();
    }
    bool consistent() const {
        return decoder_->platform() == relay_->platform() &&
               relay_->platform() == watchdog_->platform();
    }
    int pulses() const { return relay_->pulses(); }
    int kicks() const { return watchdog_->kicks(); }

private:
    std::unique_ptr<Decoder> decoder_;
    std::unique_ptr<RelayOutput> relay_;
    std::unique_ptr<Watchdog> watchdog_;
};

int boot(Gateway& g) {
    const int accepted = g.start_streams(kRequestedStreams);
    for (int i = 0; i < kPulses; ++i) g.trigger_relay();
    for (int i = 0; i < kKicks; ++i) g.heartbeat();
    return accepted;
}

int main() {
    Gateway arm({Platform::ArmAppliance, true});
    CHECK_EQ(boot(arm), kArmAccepted);
    CHECK_EQ(arm.signature(), std::string("hw-decoder+gpio-relay+hw-watchdog"));
    CHECK_EQ(arm.pulses(), kPulses);
    CHECK_EQ(arm.kicks(), kKicks);

    Gateway vm({Platform::X86Vm, false});
    CHECK_EQ(boot(vm), kVmAccepted);
    CHECK_EQ(vm.signature(), std::string("sw-decoder+sim-relay+systemd-watchdog"));

    // The hurt, demonstrated: an inconsistent family is perfectly constructible.
    Gateway broken({Platform::X86Vm, /*hw_decode=*/true});
    CHECK(!broken.consistent());
    CHECK_EQ(broken.signature(), std::string("hw-decoder+sim-relay+systemd-watchdog"));

    std::cout << "14-abstract-factory/naive: arm=" << kArmAccepted << " vm=" << kVmAccepted
              << " pulses=" << kPulses << " kicks=" << kKicks << " (mixed family constructible)\n";
}
