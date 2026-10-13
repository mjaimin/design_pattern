// 08 Command — commands as plain data: std::variant + std::visit.
//
// pattern.cpp's commands hold *references* to devices. That is fine in-process,
// but a command that references a Relay& cannot be written to a journal,
// sent from the cloud, or replayed after a reboot.
//
// Here a command is a plain struct naming its target by id:
//     SetRelay{"siren", true}   GotoPreset{"ptz-1", 3}
// It has value semantics, a one-line text form (to_line / parse_line), and a
// separate Executor resolves ids to devices and runs it. Executing returns the
// inverse command — also plain data — so the undo history is serialisable too.
//
// Wins when: commands cross a process/network/time boundary (MQTT, a REST
// call, a persisted job queue). The set of command types is closed, which
// std::variant enforces: add a type and the compiler points at every visitor.
// The std::function form (a queue of std::function<void()> jobs) is the other
// common modern form — it fits fire-and-forget work on a thread pool, but a
// lambda can't be logged, inspected, serialised or inverted.

#include <deque>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "check.hpp"
#include "devices.hpp"

struct SetRelay {
    std::string relay;
    bool on;
};
struct GotoPreset {
    std::string camera;
    int preset;
};
using Command = std::variant<SetRelay, GotoPreset>;

template <class... Fs>
struct overloaded : Fs... {
    using Fs::operator()...;
};

// ---- Wire format: one line per command ----------------------------------------
std::string to_line(const Command& c) {
    return std::visit(overloaded{
                          [](const SetRelay& s) { return "relay " + s.relay + (s.on ? " on" : " off"); },
                          [](const GotoPreset& g) { return "ptz " + g.camera + " " + std::to_string(g.preset); },
                      },
                      c);
}

std::optional<Command> parse_line(const std::string& line) {
    std::istringstream in(line);
    std::string verb, target, arg;
    if (!(in >> verb >> target >> arg)) return std::nullopt;
    if (verb == "relay" && (arg == "on" || arg == "off")) return SetRelay{target, arg == "on"};
    if (verb == "ptz") return GotoPreset{target, std::stoi(arg)};
    return std::nullopt;
}

// ---- Executor: resolves ids, retries, logs, returns the inverse ----------------
class Executor {
public:
    Executor(std::map<std::string, Relay*> relays, std::map<std::string, PtzCamera*> cams)
        : relays_(std::move(relays)), cams_(std::move(cams)) {}

    std::optional<Command> execute(const Command& c) {
        for (int i = 0; i < kMaxAttempts; ++i) {
            if (auto inverse = try_once(c)) {
                log_.push_back(describe(c));
                return inverse;
            }
        }
        log_.push_back(describe(c) + " FAILED");
        return std::nullopt;
    }

    const std::vector<std::string>& audit_log() const { return log_; }

private:
    std::optional<Command> try_once(const Command& c) {
        return std::visit(overloaded{
                              [&](const SetRelay& s) -> std::optional<Command> {
                                  Relay& r = *relays_.at(s.relay);
                                  const bool previous = r.is_on();
                                  if (!r.set(s.on)) return std::nullopt;
                                  return SetRelay{s.relay, previous};
                              },
                              [&](const GotoPreset& g) -> std::optional<Command> {
                                  PtzCamera& cam = *cams_.at(g.camera);
                                  const int previous = cam.preset();
                                  if (!cam.goto_preset(g.preset)) return std::nullopt;
                                  return GotoPreset{g.camera, previous};
                              },
                          },
                          c);
    }

    static std::string describe(const Command& c) {
        return std::visit(overloaded{
                              [](const SetRelay& s) { return s.relay + (s.on ? ": on" : ": off"); },
                              [](const GotoPreset& g) { return g.camera + ": preset " + std::to_string(g.preset); },
                          },
                          c);
    }

    std::map<std::string, Relay*> relays_;
    std::map<std::string, PtzCamera*> cams_;
    std::vector<std::string> log_;
};

int main() {
    Relay siren("siren", /*drop_first_n=*/1);
    Relay flood("floodlight");
    PtzCamera ptz("ptz-1");
    Executor exec({{"siren", &siren}, {"floodlight", &flood}}, {{"ptz-1", &ptz}});

    // The alarm rule arrives as text — e.g. pushed from the cloud, or replayed from a journal.
    const std::vector<std::string> wire = {"relay siren on", "relay floodlight on", "ptz ptz-1 3"};

    std::vector<Command> undo_stack;
    for (const auto& line : wire) {
        const auto cmd = parse_line(line);
        CHECK(cmd.has_value());
        CHECK_EQ(to_line(*cmd), line);  // round-trips: a command is data
        if (auto inv = exec.execute(*cmd)) undo_stack.push_back(*inv);
    }
    CHECK(siren.is_on() && flood.is_on());
    CHECK_EQ(ptz.preset(), 3);

    // The undo stack is data too: it could be persisted before the operator acks.
    CHECK_EQ(to_line(undo_stack.back()), std::string("ptz ptz-1 1"));
    while (!undo_stack.empty()) {
        exec.execute(undo_stack.back());
        undo_stack.pop_back();
    }

    // Same numbers as naive.cpp and pattern.cpp.
    CHECK(exec.audit_log() == kAuditLog);
    CHECK_EQ(siren.attempts(), kSirenAttempts);
    CHECK_EQ(flood.attempts(), kFloodAttempts);
    CHECK(!parse_line("relay siren maybe"));

    std::cout << "08-command/modern: variant commands parsed from text match pattern.cpp\n";
}
