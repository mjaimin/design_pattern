// 08 Command — classic form (command objects + an invoker with history).
//
// Force: device actions are issued by many sources (rules engine, operator UI,
//        scheduler) but must be executed by one worker, retried, audited and
//        undone the same way. The action must become a value that can be
//        queued, logged and reversed, separate from whoever asked for it.
//
// Roles
//   Command          : Command        — execute(), inverse(), describe()
//   Concrete commands: SetRelay, GotoPreset — bind one receiver + one parameter
//   Receivers        : Relay, PtzCamera (devices.hpp) — do the actual work
//   Invoker          : CommandQueue   — queues, retries, logs, keeps history, undoes
//   Client           : on_intrusion() — creates commands and submits them
//
// Undo is modelled as "execute the inverse command": the inverse goes through
// the same retry + audit path as any other command, so undo is never a
// second-class code path.

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "devices.hpp"

// ---- Command --------------------------------------------------------------------
class Command {
public:
    virtual ~Command() = default;
    virtual bool execute() = 0;                               // false = device did not ACK
    virtual std::unique_ptr<Command> inverse() const = 0;     // valid after a successful execute()
    virtual std::string describe() const = 0;
};

// ---- Concrete commands -----------------------------------------------------------
class SetRelay final : public Command {
public:
    SetRelay(Relay& relay, bool on) : relay_(relay), on_(on) {}

    bool execute() override {
        previous_ = relay_.is_on();  // captured at execution time, not at creation time
        return relay_.set(on_);
    }
    std::unique_ptr<Command> inverse() const override { return std::make_unique<SetRelay>(relay_, previous_); }
    std::string describe() const override { return relay_.name() + (on_ ? ": on" : ": off"); }

private:
    Relay& relay_;
    bool on_;
    bool previous_ = false;
};

class GotoPreset final : public Command {
public:
    GotoPreset(PtzCamera& cam, int preset) : cam_(cam), preset_(preset) {}

    bool execute() override {
        previous_ = cam_.preset();
        return cam_.goto_preset(preset_);
    }
    std::unique_ptr<Command> inverse() const override { return std::make_unique<GotoPreset>(cam_, previous_); }
    std::string describe() const override { return cam_.name() + ": preset " + std::to_string(preset_); }

private:
    PtzCamera& cam_;
    int preset_;
    int previous_ = 0;
};

// ---- Invoker ---------------------------------------------------------------------
class CommandQueue {
public:
    // Any source may submit; nothing touches a device until run_pending().
    void submit(std::unique_ptr<Command> c) { pending_.push_back(std::move(c)); }

    // The single device worker: retry + audit live here, once, for every command.
    void run_pending() {
        while (!pending_.empty()) {
            auto c = std::move(pending_.front());
            pending_.pop_front();
            if (run(*c)) history_.push_back(std::move(c));
        }
    }

    void undo_last(std::size_t n) {
        for (; n > 0 && !history_.empty(); --n) {
            auto inv = history_.back()->inverse();
            history_.pop_back();
            run(*inv);
        }
    }

    std::size_t pending() const { return pending_.size(); }
    const std::vector<std::string>& audit_log() const { return log_; }

private:
    bool run(Command& c) {
        for (int i = 0; i < kMaxAttempts; ++i) {
            if (c.execute()) {
                log_.push_back(c.describe());
                return true;
            }
        }
        log_.push_back(c.describe() + " FAILED");
        return false;
    }

    std::deque<std::unique_ptr<Command>> pending_;
    std::vector<std::unique_ptr<Command>> history_;
    std::vector<std::string> log_;
};

// ---- Client: a rule reacting to an intrusion --------------------------------------
void on_intrusion(CommandQueue& q, Relay& siren, Relay& flood, PtzCamera& ptz) {
    q.submit(std::make_unique<SetRelay>(siren, true));
    q.submit(std::make_unique<SetRelay>(flood, true));
    q.submit(std::make_unique<GotoPreset>(ptz, 3));
}

int main() {
    Relay siren("siren", /*drop_first_n=*/1);
    Relay flood("floodlight");
    PtzCamera ptz("ptz-1");
    CommandQueue q;

    on_intrusion(q, siren, flood, ptz);
    CHECK_EQ(q.pending(), 3);
    CHECK(!siren.is_on());  // submitted, not yet executed: request decoupled from execution

    q.run_pending();
    CHECK(siren.is_on() && flood.is_on());
    CHECK_EQ(ptz.preset(), 3);

    q.undo_last(3);  // operator acknowledges the alarm
    CHECK(!siren.is_on() && !flood.is_on());
    CHECK_EQ(ptz.preset(), 1);

    // Same numbers as naive.cpp.
    CHECK(q.audit_log() == kAuditLog);
    CHECK_EQ(siren.attempts(), kSirenAttempts);
    CHECK_EQ(flood.attempts(), kFloodAttempts);

    std::cout << "08-command/pattern: log=" << q.audit_log().size() << " siren_attempts=" << siren.attempts()
              << " flood_attempts=" << flood.attempts() << "\n";
}
