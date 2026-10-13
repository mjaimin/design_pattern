// 11 Composite — the naive version.
//
// The site layout is modelled with one struct per level: Site holds Zones,
// Zone holds Cameras and Relays. Every operation (arm, count, health, find) is
// written once per level, as nested loops that know the exact shape.
//
// Where it hurts:
//   * A customer has a building with floors with zones. That is a new level:
//     a new struct, and a new overload of *every* operation.
//   * A new leaf type (a door sensor) means editing every Zone function to
//     loop over one more vector.
//   * "Arm this node" needs a different function depending on whether the
//     node is a site, a zone or a single device; callers must know which.
// Smells: Duplicate Code (same traversal per operation and per level) and
// Shotgun Surgery (one new device type or level touches every function).

#include <string>
#include <vector>

#include "check.hpp"
#include "fixture.hpp"

struct Camera {
    std::string name;
    bool online;
    bool armed = false;
};

struct Relay {
    std::string name;
    bool online;
    bool armed = false;
};

struct Zone {
    std::string name;
    std::vector<Camera> cameras;
    std::vector<Relay> relays;
};

struct Site {
    std::string name;
    std::vector<Zone> zones;
};

// ---- arm: one per level ------------------------------------------------------
void set_armed(Zone& z, bool armed) {
    for (auto& c : z.cameras) c.armed = armed;
    for (auto& r : z.relays) r.armed = armed;
}
void set_armed(Site& s, bool armed) {
    for (auto& z : s.zones) set_armed(z, armed);
}

// ---- counts: one per (operation × level) -------------------------------------
int device_count(const Zone& z) { return static_cast<int>(z.cameras.size() + z.relays.size()); }
int device_count(const Site& s) {
    int n = 0;
    for (const auto& z : s.zones) n += device_count(z);
    return n;
}

int online_count(const Zone& z) {
    int n = 0;
    for (const auto& c : z.cameras) n += c.online;
    for (const auto& r : z.relays) n += r.online;
    return n;
}
int online_count(const Site& s) {
    int n = 0;
    for (const auto& z : s.zones) n += online_count(z);
    return n;
}

int armed_count(const Zone& z) {
    int n = 0;
    for (const auto& c : z.cameras) n += c.armed;
    for (const auto& r : z.relays) n += r.armed;
    return n;
}
int armed_count(const Site& s) {
    int n = 0;
    for (const auto& z : s.zones) n += armed_count(z);
    return n;
}

// ---- health: one per level ---------------------------------------------------
Health health(const Zone& z) {
    HealthTally t;
    for (const auto& c : z.cameras) t.add(c.online ? Health::Ok : Health::Down);
    for (const auto& r : z.relays) t.add(r.online ? Health::Ok : Health::Down);
    return t.result();
}
Health health(const Site& s) {
    HealthTally t;
    for (const auto& z : s.zones) t.add(health(z));
    return t.result();
}

Zone* find_zone(Site& s, const std::string& name) {
    for (auto& z : s.zones)
        if (z.name == name) return &z;
    return nullptr;
}

int main() {
    Site site{"site-1",
              {
                  {"entrance", {{"e1", true}, {"e2", true}}, {{"door-1", true}}},
                  {"parking", {{"p1", true}, {"p2", false}, {"p3", true}}, {}},
                  {"warehouse", {{"w1", false}}, {{"siren-1", true}}},
              }};

    CHECK_EQ(device_count(site), kDevices);
    CHECK_EQ(online_count(site), kOnline);
    CHECK_EQ(health(*find_zone(site, "parking")), kParkingHealth);
    CHECK_EQ(health(site), kSiteHealth);

    set_armed(*find_zone(site, "parking"), true);
    CHECK_EQ(armed_count(site), kArmedAfterParking);
    set_armed(site, true);  // a different overload: caller must know it's a Site
    CHECK_EQ(armed_count(site), kArmedAfterSite);
    set_armed(*find_zone(site, "entrance"), false);
    CHECK_EQ(armed_count(site), kArmedAfterDisarm);

    std::cout << "11-composite/naive: devices=" << kDevices << " online=" << kOnline
              << " site=" << kSiteHealth << " armed=" << kArmedAfterDisarm << "\n";
}
