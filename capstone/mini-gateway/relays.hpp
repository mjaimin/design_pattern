#pragma once
// COMMAND (ch. 08): relay actions as objects — queued by whoever decides,
// executed later by whoever owns the hardware, and logged on the way.

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "domain.hpp"

namespace mg {

// NO PATTERN: one relay board, used directly. An interface over it would have
// a single implementation; extract one when a second board type is real.
class RelayBoard {
public:
    void pulse(int channel, gw::TimestampMs duration) {
        ++activations_[channel];
        on_time_[channel] += duration;
    }
    int activations(int channel) const {
        auto it = activations_.find(channel);
        return it == activations_.end() ? 0 : it->second;
    }

private:
    std::map<int, int> activations_;
    std::map<int, gw::TimestampMs> on_time_;
};

class RelayCommand {
public:
    virtual ~RelayCommand() = default;
    virtual void execute(RelayBoard& board) = 0;
    virtual std::string describe() const = 0;
};

class PulseRelay final : public RelayCommand {
public:
    PulseRelay(int channel, gw::TimestampMs duration, std::string reason)
        : channel_(channel), duration_(duration), reason_(std::move(reason)) {}
    void execute(RelayBoard& board) override { board.pulse(channel_, duration_); }
    std::string describe() const override {
        return "pulse relay " + std::to_string(channel_) + " for " +
               std::to_string(duration_ / 1000) + "s (" + reason_ + ")";
    }

private:
    int channel_;
    gw::TimestampMs duration_;
    std::string reason_;
};

// Invoker: decouples "decide to act" (in an event handler) from "act"
// (once per tick, on the thread that owns the relay hardware).
class CommandQueue {
public:
    void push(std::unique_ptr<RelayCommand> c) { pending_.push_back(std::move(c)); }
    void drain(RelayBoard& board) {
        while (!pending_.empty()) {
            auto cmd = std::move(pending_.front());
            pending_.pop_front();
            cmd->execute(board);
            log_.push_back(cmd->describe());
        }
    }
    const std::vector<std::string>& log() const { return log_; }

private:
    std::deque<std::unique_ptr<RelayCommand>> pending_;
    std::vector<std::string> log_;
};

}  // namespace mg
