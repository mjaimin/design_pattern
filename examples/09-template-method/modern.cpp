// 09 Template Method — static form with CRTP (no vtable).
//
// CRTP: `class VendorA : public Onboarding<VendorA>`. The base knows the derived
// type at compile time, so the template method calls the steps directly —
// static_cast<Vendor&>(*this).probe() — and every call can be inlined.
//
// Two things the virtual form cannot do as neatly:
//   - The hook needs no default implementation in the base: `if constexpr
//     (requires { v.before_configure(); })` calls it only if the vendor has one.
//   - A missing primitive operation is a compile error at the point of use,
//     not a pure-virtual-call crash.
// What it gives up: no common runtime base. You cannot keep a
// std::vector<Onboarding*> of mixed vendors (Onboarding<VendorA> and
// Onboarding<VendorB> are unrelated types), and choosing the vendor at runtime
// needs something else (a variant or a virtual wrapper on top).
//
// Wins when: the set of variants is known at compile time and the skeleton is
// hot or tiny (per-frame/per-packet processing). For onboarding — runtime
// vendor choice, called once per camera — pattern.cpp's NVI form is the right call.

#include <string>
#include <vector>

#include "check.hpp"
#include "onboarding.hpp"

template <class Vendor>
class Onboarding {
public:
    // The template method.
    Profile onboard() {
        Vendor& v = static_cast<Vendor&>(*this);
        v.probe();
        v.authenticate();
        const auto profiles = v.fetch_profiles();
        const Profile chosen = select(profiles);
        if constexpr (requires { v.before_configure(); }) v.before_configure();  // optional hook
        v.configure(chosen);
        verify(chosen);
        return chosen;
    }

    const std::vector<std::string>& log() const { return log_; }

protected:
    Onboarding() = default;  // only usable as a base
    void note(std::string line) { log_.push_back(std::move(line)); }

private:
    Profile select(const std::vector<Profile>& profiles) {
        Profile best{0, 0, 0};
        for (const auto& p : profiles)
            if (p.height <= kMaxHeight && p.width * p.height > best.width * best.height) best = p;
        note("select " + label(best));
        return best;
    }
    void verify(const Profile& p) { note("verify stream " + label(p)); }

    std::vector<std::string> log_;
};

class VendorA : public Onboarding<VendorA> {
    friend class Onboarding<VendorA>;  // steps stay private; only the skeleton may call them

    void probe() { note("A: GET /api/device-info"); }
    void authenticate() { note("A: basic auth"); }
    std::vector<Profile> fetch_profiles() {
        note("A: list " + std::to_string(kVendorAProfiles.size()) + " profiles");
        return kVendorAProfiles;
    }
    void configure(const Profile& p) { note("A: PUT stream " + label(p)); }
    // no before_configure(): the hook is simply skipped
};

class VendorB : public Onboarding<VendorB> {
    friend class Onboarding<VendorB>;

    void probe() { note("B: GetDeviceInformation"); }
    void authenticate() { note("B: digest auth"); }
    std::vector<Profile> fetch_profiles() {
        note("B: list " + std::to_string(kVendorBProfiles.size()) + " profiles");
        return kVendorBProfiles;
    }
    void before_configure() { note("B: disable smart codec"); }
    void configure(const Profile& p) { note("B: SetEncoder " + label(p)); }
};

int main() {
    VendorA a;
    VendorB b;
    const Profile pa = a.onboard();
    const Profile pb = b.onboard();

    CHECK(a.log() == kVendorALog);
    CHECK(b.log() == kVendorBLog);
    CHECK_EQ(label(pa), std::string("1920x1080@15"));
    CHECK_EQ(label(pb), std::string("1280x720@25"));

    std::cout << "09-template-method/modern: CRTP static template method matches pattern.cpp\n";
}
