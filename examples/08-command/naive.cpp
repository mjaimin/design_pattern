// 08 Command — the naive version (call the devices directly).
//
// AlarmController reacts to an intrusion by calling the devices itself, and
// handles the operator's "acknowledge" by reversing what it did.
//
// Where it hurts — every action needs the same three things, written by hand
// at every call site:
//   1. retry until the device ACKs          (loop copy-pasted per action)
//   2. an audit-log line                    (string copy-pasted per action)
//   3. remember the previous state for undo (one member field per action)
// And the action itself is not a thing you can hold:
//   - it cannot be queued to a single device-worker thread,
//   - it cannot be persisted and replayed after a reboot,
//   - the UI, the scheduler and the rules engine each need their own copy of
//     "set relay with retry and log", and they will drift apart.
// Smells: Duplicate Code, Shotgun Surgery (changing the retry policy touches
// every call site), and Temporary Field (prev_* only meaningful after an alarm).

#include <string>
#include <vector>

#include "check.hpp"
#include "devices.hpp"

class AlarmController {
public:
    AlarmController(Relay& siren, Relay& flood, PtzCamera& ptz) : siren_(siren), flood_(flood), ptz_(ptz) {}

    void on_intrusion() {
        prev_siren_ = siren_.is_on();
        set_with_retry(siren_, true);

        prev_flood_ = flood_.is_on();
        set_with_retry(flood_, true);

        prev_preset_ = ptz_.preset();
        ptz_.goto_preset(3);
        log_.push_back(ptz_.name() + ": preset 3");
    }

    void on_acknowledge() {  // reverse order, by hand
        ptz_.goto_preset(prev_preset_);
        log_.push_back(ptz_.name() + ": preset " + std::to_string(prev_preset_));
        set_with_retry(flood_, prev_flood_);
        set_with_retry(siren_, prev_siren_);
    }

    const std::vector<std::string>& audit_log() const { return log_; }

private:
    // Factored out here — in a real codebase this loop is pasted into the UI
    // handler, the scheduler and the rules engine as well.
    void set_with_retry(Relay& r, bool on) {
        for (int i = 0; i < kMaxAttempts; ++i) {
            if (r.set(on)) {
                log_.push_back(r.name() + (on ? ": on" : ": off"));
                return;
            }
        }
        log_.push_back(r.name() + (on ? ": on" : ": off") + " FAILED");
    }

    Relay& siren_;
    Relay& flood_;
    PtzCamera& ptz_;
    bool prev_siren_ = false;
    bool prev_flood_ = false;
    int prev_preset_ = 0;
    std::vector<std::string> log_;
};

int main() {
    Relay siren("siren", /*drop_first_n=*/1);
    Relay flood("floodlight");
    PtzCamera ptz("ptz-1");
    AlarmController ctl(siren, flood, ptz);

    ctl.on_intrusion();
    CHECK(siren.is_on() && flood.is_on());
    CHECK_EQ(ptz.preset(), 3);

    ctl.on_acknowledge();
    CHECK(!siren.is_on() && !flood.is_on());
    CHECK_EQ(ptz.preset(), 1);

    CHECK(ctl.audit_log() == kAuditLog);
    CHECK_EQ(siren.attempts(), kSirenAttempts);
    CHECK_EQ(flood.attempts(), kFloodAttempts);

    std::cout << "08-command/naive: log=" << ctl.audit_log().size() << " siren_attempts=" << siren.attempts()
              << " flood_attempts=" << flood.attempts() << "\n";
}
