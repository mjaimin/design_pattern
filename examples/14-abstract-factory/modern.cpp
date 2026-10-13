// 14 Abstract Factory — compile-time family: a "platform traits" type.
//
// When the platform is fixed per *build* (one firmware image for the ARM
// appliance, another for the VM), the family can be chosen by the compiler:
// a traits struct names the three concrete types, and Gateway<Platform>
// instantiates them directly.
//
//   + no virtual calls, no heap for the factory, unused family not linked in
//   + still impossible to mix: the family is one template argument
//   - cannot choose at runtime; Gateway<Arm> and Gateway<Vm> are unrelated types
//
// The products are the same classes as before, but held by value and called
// directly: their virtual base is simply not used. A concept describes what a
// family must provide.

#include <algorithm>
#include <concepts>
#include <string>

#include "check.hpp"
#include "platform_parts.hpp"

// A family = a struct of type aliases.
struct ArmAppliance {
    using Decoder = HwDecoder;
    using Relay = GpioRelay;
    using Watchdog = HwWatchdog;
};

struct X86Vm {
    using Decoder = SwDecoder;
    using Relay = SimulatedRelay;
    using Watchdog = SystemdWatchdog;
};

template <class P>
concept PlatformFamily = requires {
    typename P::Decoder;
    typename P::Relay;
    typename P::Watchdog;
} && std::default_initializable<typename P::Decoder> && std::default_initializable<typename P::Relay> &&
                         std::default_initializable<typename P::Watchdog>;

template <PlatformFamily P>
class Gateway {
public:
    int start_streams(int requested) { return std::min(requested, decoder_.max_streams()); }
    void trigger_relay() { relay_.pulse(); }
    void heartbeat() { watchdog_.kick(); }
    std::string signature() const {
        return decoder_.name() + "+" + relay_.name() + "+" + watchdog_.name();
    }
    int pulses() const { return relay_.pulses(); }
    int kicks() const { return watchdog_.kicks(); }

private:
    typename P::Decoder decoder_;  // by value: the factory *is* the type list
    typename P::Relay relay_;
    typename P::Watchdog watchdog_;
};

template <class G>
int boot(G& g) {
    const int accepted = g.start_streams(kRequestedStreams);
    for (int i = 0; i < kPulses; ++i) g.trigger_relay();
    for (int i = 0; i < kKicks; ++i) g.heartbeat();
    return accepted;
}

static_assert(!PlatformFamily<int>);

int main() {
    Gateway<ArmAppliance> arm;
    CHECK_EQ(boot(arm), kArmAccepted);
    CHECK_EQ(arm.signature(), std::string("hw-decoder+gpio-relay+hw-watchdog"));
    CHECK_EQ(arm.pulses(), kPulses);
    CHECK_EQ(arm.kicks(), kKicks);

    Gateway<X86Vm> vm;
    CHECK_EQ(boot(vm), kVmAccepted);
    CHECK_EQ(vm.signature(), std::string("sw-decoder+sim-relay+systemd-watchdog"));

    std::cout << "14-abstract-factory/modern: arm=" << kArmAccepted << " vm=" << kVmAccepted
              << " (family chosen at compile time)\n";
}
