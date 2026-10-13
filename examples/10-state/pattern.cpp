// 10 State — classic form (one class per state, virtual event handlers).
//
// Force: what each event means (start, connect_ok, stream_lost, tick, …)
//        changes wholesale with the link's state, and the state changes
//        itself in response to those events. The CameraLink's public API and
//        its callers must not change when a state is added.
//
// Roles
//   Context        : CameraLink  — forwards every event to the current state
//   State          : LinkState   — one virtual handler per event
//   Concrete states: Disconnected, Connecting, Streaming, Reconnecting, Offline
//
// Design choice: a handler *returns* the next state (or nullptr to stay)
// instead of calling link.set_state(new X) itself. The context swaps the state
// only after the handler has returned, so a state object is never destroyed
// while one of its own member functions is still running.

#include <memory>
#include <string>
#include <string_view>

#include "check.hpp"
#include "link_script.hpp"

class CameraLink;

// ---- State -------------------------------------------------------------------
class LinkState {
public:
    using Next = std::unique_ptr<LinkState>;

    virtual ~LinkState() = default;
    virtual std::string_view name() const = 0;
    virtual void on_enter(CameraLink&) {}  // entry action, default none

    // Default for every event: ignore it and stay. States override only the
    // events that mean something to them.
    virtual Next start() { return nullptr; }
    virtual Next connect_ok() { return nullptr; }
    virtual Next connect_failed(gw::TimestampMs) { return nullptr; }
    virtual Next stream_lost(gw::TimestampMs) { return nullptr; }
    virtual Next tick(gw::TimestampMs) { return nullptr; }
    virtual Next stop();  // every state except Disconnected → Disconnected
    virtual bool forwards_frames() const { return false; }
};

// ---- Concrete states ---------------------------------------------------------
class Disconnected final : public LinkState {
public:
    std::string_view name() const override { return "Disconnected"; }
    Next start() override;
    Next stop() override { return nullptr; }
};

class Connecting final : public LinkState {
public:
    explicit Connecting(int failures) : failures_(failures) {}
    std::string_view name() const override { return "Connecting"; }
    void on_enter(CameraLink& link) override;
    Next connect_ok() override;
    Next connect_failed(gw::TimestampMs ts) override;

private:
    int failures_;  // consecutive failures so far — lives in the states that use it
};

class Streaming final : public LinkState {
public:
    std::string_view name() const override { return "Streaming"; }
    bool forwards_frames() const override { return true; }
    Next stream_lost(gw::TimestampMs ts) override;
};

class Reconnecting final : public LinkState {
public:
    Reconnecting(gw::TimestampMs retry_at, int failures) : retry_at_(retry_at), failures_(failures) {}
    std::string_view name() const override { return "Reconnecting"; }
    Next tick(gw::TimestampMs ts) override {
        if (ts < retry_at_) return nullptr;  // backoff not elapsed yet
        return std::make_unique<Connecting>(failures_);
    }

private:
    gw::TimestampMs retry_at_;  // no longer a Temporary Field on the context
    int failures_;
};

class Offline final : public LinkState {
public:
    std::string_view name() const override { return "Offline"; }
    Next start() override { return std::make_unique<Connecting>(0); }  // operator retry
};

// Transitions that need classes declared later are defined out of line.
LinkState::Next LinkState::stop() { return std::make_unique<Disconnected>(); }
LinkState::Next Disconnected::start() { return std::make_unique<Connecting>(0); }
LinkState::Next Connecting::connect_ok() { return std::make_unique<Streaming>(); }
LinkState::Next Connecting::connect_failed(gw::TimestampMs ts) {
    const int failures = failures_ + 1;
    if (failures >= kMaxFailures) return std::make_unique<Offline>();
    return std::make_unique<Reconnecting>(ts + backoff(failures), failures);
}
LinkState::Next Streaming::stream_lost(gw::TimestampMs ts) {
    return std::make_unique<Reconnecting>(ts + backoff(0), 0);
}

// ---- Context -----------------------------------------------------------------
class CameraLink {
public:
    CameraLink() : state_(std::make_unique<Disconnected>()), trace_(state_->name()) {}

    void start() { apply(state_->start()); }
    void connect_ok() { apply(state_->connect_ok()); }
    void connect_failed(gw::TimestampMs ts) { apply(state_->connect_failed(ts)); }
    void stream_lost(gw::TimestampMs ts) { apply(state_->stream_lost(ts)); }
    void tick(gw::TimestampMs ts) { apply(state_->tick(ts)); }
    void stop() { apply(state_->stop()); }
    void frame() {
        if (state_->forwards_frames()) {
            ++forwarded_;
        } else {
            ++dropped_;
        }
    }

    // Called by states' entry actions.
    void count_connect_attempt() { ++attempts_; }

    int forwarded() const { return forwarded_; }
    int dropped() const { return dropped_; }
    int connect_attempts() const { return attempts_; }
    const std::string& trace() const { return trace_; }

private:
    void apply(LinkState::Next next) {
        if (!next) return;
        state_ = std::move(next);  // the old state is destroyed here, after its handler returned
        trace_ += '>';
        trace_ += state_->name();
        state_->on_enter(*this);
    }

    std::unique_ptr<LinkState> state_;
    std::string trace_;
    int forwarded_ = 0;
    int dropped_ = 0;
    int attempts_ = 0;
};

void Connecting::on_enter(CameraLink& link) { link.count_connect_attempt(); }

int main() {
    CameraLink link;
    replay(link);

    // Same numbers and the same state sequence as naive.cpp.
    CHECK_EQ(link.forwarded(), kForwarded);
    CHECK_EQ(link.dropped(), kDropped);
    CHECK_EQ(link.connect_attempts(), kConnectAttempts);
    CHECK(link.trace() == kTrace);

    std::cout << "10-state/pattern: forwarded=" << link.forwarded() << " dropped=" << link.dropped()
              << " attempts=" << link.connect_attempts() << "\n";
}
