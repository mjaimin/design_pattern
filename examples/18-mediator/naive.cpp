// 18 Mediator — the naive version.
//
// An alarm site: a keypad, a door contact sensor, a siren, two cameras and a
// door lock. The *rules* ("when armed and the door opens, sound the siren and
// record") are spread across the devices that emit events: Keypad drives the
// lock, siren and cameras; DoorSensor asks Keypad whether the site is armed
// and then drives the siren and cameras itself.
//
// Where it hurts — add a strobe light that flashes with the siren:
//   1. Keypad needs a Strobe& (to switch it off on disarm)
//   2. DoorSensor needs a Strobe& (to switch it on)
//   3. both constructors, and every place that builds them, change
// Add a second door sensor and its constructor needs the keypad, the siren,
// the cameras and the strobe. Each new device adds wiring to every other.
// Smells: Inappropriate Intimacy (DoorSensor reads Keypad's armed state),
// Shotgun Surgery (one new rule edits many classes), and the "who knows whom"
// graph grows as N×N.

#include <vector>

#include "check.hpp"
#include "devices.hpp"

class Keypad {
public:
    Keypad(Siren& siren, std::vector<Camera*> cams, DoorLock& lock)
        : siren_(siren), cams_(std::move(cams)), lock_(lock) {}

    void enter_code(int code) {
        if (code != kArmCode) return;
        armed_ = !armed_;
        if (armed_) {
            lock_.lock();
        } else {
            siren_.off();
            for (auto* c : cams_) c->stop_recording();
            lock_.unlock();
        }
    }

    bool armed() const { return armed_; }  // DoorSensor depends on this

private:
    Siren& siren_;
    std::vector<Camera*> cams_;
    DoorLock& lock_;
    bool armed_ = false;
};

class DoorSensor {
public:
    DoorSensor(const Keypad& keypad, Siren& siren, std::vector<Camera*> cams)
        : keypad_(keypad), siren_(siren), cams_(std::move(cams)) {}

    void opened() {
        if (!keypad_.armed()) return;
        siren_.on();
        for (auto* c : cams_) c->start_recording();
    }

private:
    const Keypad& keypad_;
    Siren& siren_;
    std::vector<Camera*> cams_;
};

int main() {
    Siren siren;
    Camera entrance("cam-entrance"), lobby("cam-lobby");
    DoorLock lock;
    Keypad keypad(siren, {&entrance, &lobby}, lock);
    DoorSensor door(keypad, siren, {&entrance, &lobby});

    door.opened();              // disarmed: nothing happens
    keypad.enter_code(kArmCode);  // arm
    door.opened();              // forced entry: siren + recording
    door.opened();              // already alarming: no new activation
    keypad.enter_code(1111);    // wrong code: ignored
    keypad.enter_code(kArmCode);  // disarm
    door.opened();              // disarmed again: nothing

    CHECK_EQ(siren.activations(), kSirenActivations);
    CHECK_EQ(entrance.clips(), kClipsPerCamera);
    CHECK_EQ(lobby.clips(), kClipsPerCamera);
    CHECK_EQ(lock.toggles(), kLockToggles);
    CHECK(!siren.active() && !entrance.recording() && !lock.locked());

    std::cout << "18-mediator/naive: siren=" << siren.activations()
              << " clips=" << entrance.clips() + lobby.clips() << " lock_toggles=" << lock.toggles()
              << "\n";
}
