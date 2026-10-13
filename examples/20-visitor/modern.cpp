// 20 Visitor — modern form: std::variant + std::visit + overloaded{}.
//
// Devices become plain value types with no common base. The closed set of
// types is spelled out once, in the variant. Each operation is a set of
// lambdas, one per alternative, combined with the `overloaded` helper.
//
// What you gain over pattern.cpp:
//   - no accept(), no visitor base class, no forward declarations;
//   - devices stored by value in a std::vector — no heap per device;
//   - an operation is local code, not a class;
//   - a missing alternative is still a compile error (no lambda matches),
//     unless you deliberately add a generic `[](const auto&)` fallback.
// What you give up:
//   - the set of types is closed: adding Nvr means editing the variant
//     (and every exhaustive visit) — the same trade-off as classic Visitor;
//   - recursive structures need care (Gateway holds a vector of the variant,
//     it is not *in* the variant here).
// This is exactly the "std::variant column" of the table in 00 §4.

#include <string>
#include <variant>
#include <vector>

#include "check.hpp"
#include "expected.hpp"

struct Camera {
    std::string id;
    bool online;
    bool ai;
};
struct Relay {
    std::string id;
    bool online;
};
struct Sensor {
    std::string id;
    bool online;
};

using Device = std::variant<Camera, Relay, Sensor>;

struct Gateway {
    std::string id;
    bool online;
    std::vector<Device> devices;
};

// The standard C++17/20 helper: inherit every lambda's operator().
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

int licences(const std::vector<Gateway>& site) {
    int total = 0;
    for (const auto& g : site)
        for (const auto& d : g.devices)
            total += std::visit(overloaded{
                                    [](const Camera& c) { return c.ai ? 2 : 1; },
                                    [](const Relay&) { return 0; },
                                    [](const Sensor&) { return 0; },
                                },
                                d);
    return total;
}

std::vector<std::string> offline(const std::vector<Gateway>& site) {
    std::vector<std::string> out;
    for (const auto& g : site) {
        if (!g.online) out.push_back(g.id);
        for (const auto& d : g.devices)
            // Every alternative has id/online: one generic lambda covers all.
            std::visit([&](const auto& x) { if (!x.online) out.push_back(x.id); }, d);
    }
    return out;
}

std::string csv(const std::vector<Gateway>& site) {
    auto row = [](const char* kind, const std::string& id, bool online) {
        return std::string(kind) + "," + id + "," + (online ? "online" : "offline") + "\n";
    };
    std::string out;
    for (const auto& g : site) {
        out += row("gateway", g.id, g.online);
        for (const auto& d : g.devices)
            out += std::visit(overloaded{
                                  [&](const Camera& c) { return row("camera", c.id, c.online); },
                                  [&](const Relay& r) { return row("relay", r.id, r.online); },
                                  [&](const Sensor& s) { return row("sensor", s.id, s.online); },
                              },
                              d);
    }
    return out;
}

int main() {
    const std::vector<Gateway> site = {
        {"gw-1", true,
         {Camera{"cam-a", true, true}, Camera{"cam-b", false, false}, Relay{"r1", true},
          Sensor{"s1", false}}},
        {"gw-2", true, {Camera{"cam-c", true, false}, Relay{"r2", true}}},
    };

    CHECK_EQ(licences(site), kLicences);
    CHECK(offline(site) == kOffline);
    CHECK_EQ(csv(site), kCsv);

    std::cout << "20-visitor/modern: std::visit results match pattern.cpp\n";
}
