// 09 Template Method — classic form, written the C++ way (Non-Virtual Interface).
//
// Force: the onboarding *sequence* and the profile *selection rule* are gateway
//        policy and must exist exactly once; how to probe, authenticate,
//        list profiles and configure a stream differs per vendor.
//
// Roles
//   Abstract class      : CameraOnboarding
//   Template method     : onboard()          — public, NON-virtual: the order lives here only
//   Primitive operations: probe(), authenticate(), fetch_profiles(), configure()
//                         — private pure virtual: every vendor must supply them
//   Hook                : before_configure() — private virtual with a default no-op
//   Invariant steps     : select(), verify() — private non-virtual: vendors cannot change them
//   Concrete classes    : VendorAOnboarding, VendorBOnboarding
//
// NVI (Non-Virtual Interface): public functions are non-virtual, virtual
// functions are private. Derived classes can *override* a private virtual but
// cannot *call* it — so a vendor can replace a step, never reorder or skip one.

#include <string>
#include <vector>

#include "check.hpp"
#include "onboarding.hpp"

// ---- Abstract class --------------------------------------------------------------
class CameraOnboarding {
public:
    virtual ~CameraOnboarding() = default;

    // The template method.
    Profile onboard() {
        probe();
        authenticate();
        const auto profiles = fetch_profiles();
        const Profile chosen = select(profiles);
        before_configure();
        configure(chosen);
        verify(chosen);
        return chosen;
    }

    const std::vector<std::string>& log() const { return log_; }

protected:
    void note(std::string line) { log_.push_back(std::move(line)); }

private:
    // Primitive operations: no default, every vendor must answer.
    virtual void probe() = 0;
    virtual void authenticate() = 0;
    virtual std::vector<Profile> fetch_profiles() = 0;
    virtual void configure(const Profile& p) = 0;

    // Hook: optional extension point, does nothing unless overridden.
    virtual void before_configure() {}

    // Invariant steps: gateway policy, written once.
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

// ---- Concrete classes: only the vendor-specific steps ------------------------------
class VendorAOnboarding final : public CameraOnboarding {
private:
    void probe() override { note("A: GET /api/device-info"); }
    void authenticate() override { note("A: basic auth"); }
    std::vector<Profile> fetch_profiles() override {
        note("A: list " + std::to_string(kVendorAProfiles.size()) + " profiles");
        return kVendorAProfiles;
    }
    void configure(const Profile& p) override { note("A: PUT stream " + label(p)); }
};

class VendorBOnboarding final : public CameraOnboarding {
private:
    void probe() override { note("B: GetDeviceInformation"); }
    void authenticate() override { note("B: digest auth"); }
    std::vector<Profile> fetch_profiles() override {
        note("B: list " + std::to_string(kVendorBProfiles.size()) + " profiles");
        return kVendorBProfiles;
    }
    void before_configure() override { note("B: disable smart codec"); }  // uses the hook
    void configure(const Profile& p) override { note("B: SetEncoder " + label(p)); }
};

int main() {
    VendorAOnboarding a;
    VendorBOnboarding b;
    const Profile pa = a.onboard();
    const Profile pb = b.onboard();

    // Same logs as naive.cpp: the sequence moved into one place, behaviour did not change.
    CHECK(a.log() == kVendorALog);
    CHECK(b.log() == kVendorBLog);
    CHECK_EQ(label(pa), std::string("1920x1080@15"));
    CHECK_EQ(label(pb), std::string("1280x720@25"));

    std::cout << "09-template-method/pattern: A=" << label(pa) << " (" << a.log().size() << " steps) B="
              << label(pb) << " (" << b.log().size() << " steps)\n";
}
