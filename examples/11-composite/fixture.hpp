#pragma once
// Shared by naive.cpp, pattern.cpp and modern.cpp: the health rule and the
// expected numbers for the same site layout:
//
//   site-1
//   ├── entrance   cam e1 (online), cam e2 (online), relay door-1 (online)
//   ├── parking    cam p1 (online), cam p2 (OFFLINE), cam p3 (online)
//   └── warehouse  cam w1 (OFFLINE), relay siren-1 (online)

#include <ostream>

enum class Health { Ok, Degraded, Down };

inline std::ostream& operator<<(std::ostream& os, Health h) {
    switch (h) {
        case Health::Ok:       return os << "Ok";
        case Health::Degraded: return os << "Degraded";
        case Health::Down:     return os << "Down";
    }
    return os;
}

// Roll-up rule for a group: all children Ok → Ok, all Down → Down, else Degraded.
// An empty group is Ok.
struct HealthTally {
    int ok = 0;
    int down = 0;
    int total = 0;

    void add(Health h) {
        ++total;
        if (h == Health::Ok) ++ok;
        if (h == Health::Down) ++down;
    }
    Health result() const {
        if (ok == total) return Health::Ok;
        if (down == total) return Health::Down;
        return Health::Degraded;
    }
};

inline constexpr int kDevices = 8;
inline constexpr int kOnline = 6;
inline constexpr Health kParkingHealth = Health::Degraded;
inline constexpr Health kSiteHealth = Health::Degraded;
inline constexpr int kArmedAfterParking = 3;  // arm "parking"
inline constexpr int kArmedAfterSite = 8;     // then arm "site-1"
inline constexpr int kArmedAfterDisarm = 5;   // then disarm "entrance"
