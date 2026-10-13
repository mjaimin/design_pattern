// 09 Template Method — the naive version (one copy-pasted function per vendor).
//
// Each vendor gets its own onboarding function. They were written by copying
// the first one and changing the vendor calls.
//
// Where it hurts:
//   1. The *sequence* (probe → auth → fetch → select → configure → verify) is
//      written once per vendor. Add "sync camera clock" after auth and you edit
//      N functions — and the order can silently differ between them.
//   2. The *selection rule* is gateway policy, yet it is pasted into every
//      vendor function. Raise kMaxHeight to 1440 and you must find every copy.
//   3. Nothing forces a new vendor's function to verify the stream at the end;
//      the step is there only if whoever copied the function kept it.
// Smells: Duplicate Code, Shotgun Surgery, and an invariant (the sequence)
// that lives in N places instead of one.

#include <string>
#include <vector>

#include "check.hpp"
#include "onboarding.hpp"

Profile onboard_vendor_a(std::vector<std::string>& log) {
    log.push_back("A: GET /api/device-info");
    log.push_back("A: basic auth");
    const auto profiles = kVendorAProfiles;
    log.push_back("A: list " + std::to_string(profiles.size()) + " profiles");

    Profile best{0, 0, 0};  // copy #1 of the selection rule
    for (const auto& p : profiles)
        if (p.height <= kMaxHeight && p.width * p.height > best.width * best.height) best = p;
    log.push_back("select " + label(best));

    log.push_back("A: PUT stream " + label(best));
    log.push_back("verify stream " + label(best));
    return best;
}

Profile onboard_vendor_b(std::vector<std::string>& log) {
    log.push_back("B: GetDeviceInformation");
    log.push_back("B: digest auth");
    const auto profiles = kVendorBProfiles;
    log.push_back("B: list " + std::to_string(profiles.size()) + " profiles");

    Profile best{0, 0, 0};  // copy #2 of the selection rule
    for (const auto& p : profiles)
        if (p.height <= kMaxHeight && p.width * p.height > best.width * best.height) best = p;
    log.push_back("select " + label(best));

    log.push_back("B: disable smart codec");  // vendor B quirk, inserted by hand
    log.push_back("B: SetEncoder " + label(best));
    log.push_back("verify stream " + label(best));
    return best;
}

int main() {
    std::vector<std::string> log_a, log_b;
    const Profile a = onboard_vendor_a(log_a);
    const Profile b = onboard_vendor_b(log_b);

    CHECK(log_a == kVendorALog);
    CHECK(log_b == kVendorBLog);
    CHECK_EQ(label(a), std::string("1920x1080@15"));
    CHECK_EQ(label(b), std::string("1280x720@25"));

    std::cout << "09-template-method/naive: A=" << label(a) << " (" << log_a.size() << " steps) B=" << label(b)
              << " (" << log_b.size() << " steps)\n";
}
