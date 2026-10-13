// 10 State — the naive version.
//
// CameraLink tracks its connection state in an enum, and every event handler
// switches on that enum to decide what the event means *right now*.
//
// Where it hurts — try adding an "AuthFailed" state (bad credentials: do not
// retry until the operator fixes the password):
//   1. add an enum value and a name
//   2. visit all seven event handlers and decide what each does in AuthFailed
//   3. miss one, and the compiler is silent (a `default:` swallows it)
// The answer to "what does Reconnecting do?" is spread over seven methods;
// the answer to "what does tick() do?" is in one. For a state machine you
// usually want the opposite grouping.
// Smells: Repeated Switches (same switch in every method) and Temporary Field
// (retry_at_ means nothing unless state_ == Reconnecting).

#include <string>

#include "check.hpp"
#include "link_script.hpp"

enum class LinkState { Disconnected, Connecting, Streaming, Reconnecting, Offline };

std::string_view name(LinkState s) {
    switch (s) {
        case LinkState::Disconnected: return "Disconnected";
        case LinkState::Connecting:   return "Connecting";
        case LinkState::Streaming:    return "Streaming";
        case LinkState::Reconnecting: return "Reconnecting";
        case LinkState::Offline:      return "Offline";
    }
    return "?";
}

class CameraLink {
public:
    void start() {
        switch (state_) {
            case LinkState::Disconnected:
            case LinkState::Offline:
                failures_ = 0;
                enter(LinkState::Connecting);
                break;
            default:
                break;  // already running
        }
    }

    void connect_ok() {
        switch (state_) {
            case LinkState::Connecting: enter(LinkState::Streaming); break;
            default: break;
        }
    }

    void connect_failed(gw::TimestampMs ts) {
        switch (state_) {
            case LinkState::Connecting:
                ++failures_;
                if (failures_ >= kMaxFailures) {
                    enter(LinkState::Offline);
                } else {
                    retry_at_ = ts + backoff(failures_);
                    enter(LinkState::Reconnecting);
                }
                break;
            default:
                break;
        }
    }

    void stream_lost(gw::TimestampMs ts) {
        switch (state_) {
            case LinkState::Streaming:
                failures_ = 0;
                retry_at_ = ts + backoff(0);
                enter(LinkState::Reconnecting);
                break;
            default:
                break;
        }
    }

    void frame() {
        switch (state_) {
            case LinkState::Streaming: ++forwarded_; break;
            default: ++dropped_; break;
        }
    }

    void tick(gw::TimestampMs ts) {
        switch (state_) {
            case LinkState::Reconnecting:
                if (ts >= retry_at_) enter(LinkState::Connecting);
                break;
            default:
                break;
        }
    }

    void stop() {
        if (state_ != LinkState::Disconnected) enter(LinkState::Disconnected);
    }

    int forwarded() const { return forwarded_; }
    int dropped() const { return dropped_; }
    int connect_attempts() const { return attempts_; }
    const std::string& trace() const { return trace_; }

private:
    void enter(LinkState s) {
        state_ = s;
        trace_ += '>';
        trace_ += name(s);
        if (s == LinkState::Connecting) ++attempts_;  // entry action, hidden in here
    }

    LinkState state_ = LinkState::Disconnected;
    int failures_ = 0;              // meaningful in Connecting / Reconnecting only
    gw::TimestampMs retry_at_ = 0;  // meaningful in Reconnecting only

    int forwarded_ = 0;
    int dropped_ = 0;
    int attempts_ = 0;
    std::string trace_ = "Disconnected";
};

int main() {
    CameraLink link;
    replay(link);

    CHECK_EQ(link.forwarded(), kForwarded);
    CHECK_EQ(link.dropped(), kDropped);
    CHECK_EQ(link.connect_attempts(), kConnectAttempts);
    CHECK(link.trace() == kTrace);

    std::cout << "10-state/naive: forwarded=" << kForwarded << " dropped=" << kDropped
              << " attempts=" << kConnectAttempts << "\n";
}
