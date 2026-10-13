// 14 Abstract Factory — classic form.
//
// Force: the platform (ARM appliance, x86 VM, test rig) varies per deployment,
//        and its parts only work as a matched set; the gateway logic that
//        uses decoder + relay + watchdog must not change per platform.
//
// Roles
//   Abstract factory  : PlatformFactory   — one create method per product
//   Concrete factories: ArmApplianceFactory, X86VmFactory, FakePlatformFactory
//   Abstract products : Decoder, RelayOutput, Watchdog   (platform_parts.hpp)
//   Concrete products : HwDecoder, GpioRelay, ...         (platform_parts.hpp)
//   Client            : Gateway — sees only the abstract factory and products
//
// The platform decision is made exactly once, in make_platform(). Choosing a
// factory chooses the whole family, so a mixed set can no longer be built.

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "check.hpp"
#include "platform_parts.hpp"

// ---- Abstract factory -------------------------------------------------------
class PlatformFactory {
public:
    virtual ~PlatformFactory() = default;
    virtual std::unique_ptr<Decoder> make_decoder() const = 0;
    virtual std::unique_ptr<RelayOutput> make_relay() const = 0;
    virtual std::unique_ptr<Watchdog> make_watchdog() const = 0;
};

// ---- Concrete factories: one per family ------------------------------------
class ArmApplianceFactory final : public PlatformFactory {
public:
    std::unique_ptr<Decoder> make_decoder() const override { return std::make_unique<HwDecoder>(); }
    std::unique_ptr<RelayOutput> make_relay() const override { return std::make_unique<GpioRelay>(); }
    std::unique_ptr<Watchdog> make_watchdog() const override { return std::make_unique<HwWatchdog>(); }
};

class X86VmFactory final : public PlatformFactory {
public:
    std::unique_ptr<Decoder> make_decoder() const override { return std::make_unique<SwDecoder>(); }
    std::unique_ptr<RelayOutput> make_relay() const override { return std::make_unique<SimulatedRelay>(); }
    std::unique_ptr<Watchdog> make_watchdog() const override { return std::make_unique<SystemdWatchdog>(); }
};

// A third family costs one class and zero edits to Gateway: a test rig whose
// decoder takes exactly 2 streams, so capacity limits are easy to test.
class FakeDecoder final : public Decoder {
public:
    int max_streams() const override { return 2; }
    std::string name() const override { return "fake-decoder"; }
    Platform platform() const override { return Platform::X86Vm; }
};

class FakePlatformFactory final : public PlatformFactory {
public:
    std::unique_ptr<Decoder> make_decoder() const override { return std::make_unique<FakeDecoder>(); }
    std::unique_ptr<RelayOutput> make_relay() const override { return std::make_unique<SimulatedRelay>(); }
    std::unique_ptr<Watchdog> make_watchdog() const override { return std::make_unique<SystemdWatchdog>(); }
};

// The single place that knows which platforms exist (the composition root).
std::unique_ptr<PlatformFactory> make_platform(Platform p) {
    switch (p) {
        case Platform::ArmAppliance: return std::make_unique<ArmApplianceFactory>();
        case Platform::X86Vm: return std::make_unique<X86VmFactory>();
    }
    throw std::invalid_argument("unknown platform");
}

// ---- Client -----------------------------------------------------------------
class Gateway {
public:
    explicit Gateway(const PlatformFactory& f)
        : decoder_(f.make_decoder()), relay_(f.make_relay()), watchdog_(f.make_watchdog()) {}

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
    // Same numbers as naive.cpp.
    Gateway arm(*make_platform(Platform::ArmAppliance));
    CHECK_EQ(boot(arm), kArmAccepted);
    CHECK_EQ(arm.signature(), std::string("hw-decoder+gpio-relay+hw-watchdog"));
    CHECK_EQ(arm.pulses(), kPulses);
    CHECK_EQ(arm.kicks(), kKicks);
    CHECK(arm.consistent());

    Gateway vm(*make_platform(Platform::X86Vm));
    CHECK_EQ(boot(vm), kVmAccepted);
    CHECK_EQ(vm.signature(), std::string("sw-decoder+sim-relay+systemd-watchdog"));
    CHECK(vm.consistent());

    // The mixed family from naive.cpp has no way to be expressed here.
    // A new family plugs in without touching Gateway:
    Gateway rig{FakePlatformFactory{}};
    CHECK_EQ(boot(rig), 2);

    std::cout << "14-abstract-factory/pattern: arm=" << kArmAccepted << " vm=" << kVmAccepted
              << " pulses=" << kPulses << " kicks=" << kKicks << " fake-rig=2\n";
}
