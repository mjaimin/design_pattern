// 10 State — modern form: states as a std::variant of plain structs.
//
// Each state is a value type carrying only its own data (Reconnecting has
// retry_at, Offline has nothing). The variant can only ever hold one of them,
// so "retry_at while Streaming" is unrepresentable — the Temporary Field from
// naive.cpp is gone without any heap allocation or vtable.
//
// Handlers come in two shapes:
//   * an event that matters to ONE state  → std::get_if<State>(&state_)
//   * an event that matters to SEVERAL    → std::visit with an overload set and
//                                           a generic "ignore" fallback
//
// Trade-off vs pattern.cpp: the per-state code is grouped by *event* again
// (like naive.cpp), but the compiler now tracks which data belongs to which
// state. Adding a state means adding a variant alternative; every std::visit
// without a generic fallback stops compiling until it handles it.

#include <array>
#include <optional>
#include <string>
#include <variant>

#include "check.hpp"
#include "link_script.hpp"

struct Disconnected {};
struct Connecting { int failures; };
struct Streaming {};
struct Reconnecting { gw::TimestampMs retry_at; int failures; };
struct Offline {};

using State = std::variant<Disconnected, Connecting, Streaming, Reconnecting, Offline>;
using Next = std::optional<State>;  // nullopt = stay

// Names indexed by variant alternative — keep in the same order as State.
constexpr std::array<std::string_view, std::variant_size_v<State>> kNames = {
    "Disconnected", "Connecting", "Streaming", "Reconnecting", "Offline"};

template <class... Fs>
struct overloaded : Fs... {
    using Fs::operator()...;
};

class CameraLink {
public:
    void start() {
        apply(std::visit(overloaded{
                             [](const Disconnected&) -> Next { return Connecting{0}; },
                             [](const Offline&) -> Next { return Connecting{0}; },
                             [](const auto&) -> Next { return std::nullopt; },
                         },
                         state_));
    }

    void connect_ok() {
        if (std::holds_alternative<Connecting>(state_)) apply(Streaming{});
    }

    void connect_failed(gw::TimestampMs ts) {
        if (const auto* c = std::get_if<Connecting>(&state_)) {
            const int failures = c->failures + 1;
            if (failures >= kMaxFailures) {
                apply(Offline{});
            } else {
                apply(Reconnecting{ts + backoff(failures), failures});
            }
        }
    }

    void stream_lost(gw::TimestampMs ts) {
        if (std::holds_alternative<Streaming>(state_)) apply(Reconnecting{ts + backoff(0), 0});
    }

    void tick(gw::TimestampMs ts) {
        if (const auto* r = std::get_if<Reconnecting>(&state_); r && ts >= r->retry_at) {
            const int failures = r->failures;  // copy out before state_ is overwritten
            apply(Connecting{failures});
        }
    }

    void stop() {
        if (!std::holds_alternative<Disconnected>(state_)) apply(Disconnected{});
    }

    void frame() {
        if (std::holds_alternative<Streaming>(state_)) {
            ++forwarded_;
        } else {
            ++dropped_;
        }
    }

    int forwarded() const { return forwarded_; }
    int dropped() const { return dropped_; }
    int connect_attempts() const { return attempts_; }
    const std::string& trace() const { return trace_; }

private:
    void apply(Next next) {
        if (!next) return;
        state_ = std::move(*next);
        trace_ += '>';
        trace_ += kNames[state_.index()];
        if (std::holds_alternative<Connecting>(state_)) ++attempts_;  // entry action
    }

    State state_ = Disconnected{};
    std::string trace_ = "Disconnected";
    int forwarded_ = 0;
    int dropped_ = 0;
    int attempts_ = 0;
};

int main() {
    CameraLink link;
    replay(link);

    CHECK_EQ(link.forwarded(), kForwarded);
    CHECK_EQ(link.dropped(), kDropped);
    CHECK_EQ(link.connect_attempts(), kConnectAttempts);
    CHECK(link.trace() == kTrace);
    static_assert(sizeof(State) <= 24, "states are small values, no heap");

    std::cout << "10-state/modern: variant states match pattern.cpp\n";
}
