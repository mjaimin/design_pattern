// 18 Mediator — classic form.
//
// Force: the *coordination rules* between devices vary (armed / disarmed,
//        normal vs maintenance mode, new devices added to a site), while each
//        device must stay simple and reusable on any site.
//
// Roles
//   Mediator          : AlarmMediator          — what colleagues can report
//   Concrete mediators : AlarmController, MaintenanceController — the rules
//   Colleagues        : Keypad, DoorSensor     — report events, know only the mediator
//   (Siren, Camera, DoorLock are plain actuators driven by the mediator)
//
// Compare with naive.cpp: no device holds a reference to another device.
// The N×N wiring collapses to N→1, and every rule lives in one class.

#include <vector>

#include "check.hpp"
#include "devices.hpp"

// ---- Mediator ---------------------------------------------------------------
// Typed notifications, one per event kind. GoF's generic
// notify(sender, event) works too, but loses compile-time checking.
class AlarmMediator {
public:
    virtual ~AlarmMediator() = default;
    virtual void code_entered(int code) = 0;
    virtual void door_opened() = 0;
};

// ---- Colleagues -------------------------------------------------------------
class Keypad {
public:
    explicit Keypad(AlarmMediator& m) : mediator_(m) {}
    void enter_code(int code) { mediator_.code_entered(code); }

private:
    AlarmMediator& mediator_;
};

class DoorSensor {
public:
    explicit DoorSensor(AlarmMediator& m) : mediator_(m) {}
    void opened() { mediator_.door_opened(); }

private:
    AlarmMediator& mediator_;
};

// ---- Concrete mediator #1: the normal alarm rules ---------------------------
class AlarmController final : public AlarmMediator {
public:
    AlarmController(Siren& siren, std::vector<Camera*> cams, DoorLock& lock)
        : siren_(siren), cams_(std::move(cams)), lock_(lock) {}

    void code_entered(int code) override {
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

    void door_opened() override {
        if (!armed_) return;
        siren_.on();
        for (auto* c : cams_) c->start_recording();
    }

    bool armed() const { return armed_; }

private:
    Siren& siren_;
    std::vector<Camera*> cams_;
    DoorLock& lock_;
    bool armed_ = false;
};

// ---- Concrete mediator #2: technician on site -------------------------------
// Same devices, different rules: door openings are logged and recorded, the
// siren never sounds, the keypad is ignored. No device class changed.
class MaintenanceController final : public AlarmMediator {
public:
    explicit MaintenanceController(std::vector<Camera*> cams) : cams_(std::move(cams)) {}

    void code_entered(int) override {}
    void door_opened() override {
        ++door_events_logged_;
        for (auto* c : cams_) c->start_recording();
    }

    int door_events_logged() const { return door_events_logged_; }

private:
    std::vector<Camera*> cams_;
    int door_events_logged_ = 0;
};

int main() {
    Siren siren;
    Camera entrance("cam-entrance"), lobby("cam-lobby");
    DoorLock lock;
    AlarmController controller(siren, {&entrance, &lobby}, lock);
    Keypad keypad(controller);
    DoorSensor door(controller);

    // Same scenario as naive.cpp → same numbers.
    door.opened();
    keypad.enter_code(kArmCode);
    door.opened();
    door.opened();
    keypad.enter_code(1111);
    keypad.enter_code(kArmCode);
    door.opened();

    CHECK_EQ(siren.activations(), kSirenActivations);
    CHECK_EQ(entrance.clips(), kClipsPerCamera);
    CHECK_EQ(lobby.clips(), kClipsPerCamera);
    CHECK_EQ(lock.toggles(), kLockToggles);
    CHECK(!siren.active() && !entrance.recording() && !lock.locked() && !controller.armed());

    // Swap the rules, keep the device classes: maintenance mode.
    // (No siren is even wired in: the rule set decides which actuators matter.)
    Camera service_cam("cam-service");
    MaintenanceController maintenance({&service_cam});
    Keypad service_keypad(maintenance);
    DoorSensor service_door(maintenance);
    service_keypad.enter_code(kArmCode);  // ignored
    service_door.opened();
    service_door.opened();
    CHECK_EQ(maintenance.door_events_logged(), 2);
    CHECK_EQ(service_cam.clips(), 1);

    std::cout << "18-mediator/pattern: siren=" << siren.activations()
              << " clips=" << entrance.clips() + lobby.clips() << " lock_toggles=" << lock.toggles()
              << " maintenance_logged=" << maintenance.door_events_logged() << "\n";
}
