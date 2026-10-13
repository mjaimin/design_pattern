#pragma once
// Two fake third-party camera APIs with deliberately different shapes.
// We do not own this code and cannot change it — that is why Adapter exists.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vendor_a {

// Binary SDK: pull-based, microsecond timestamps, motion as a byte flag.
struct RawFrame {
    std::int64_t pts_us;
    std::uint8_t md;  // 1 = motion detected
};

class Device {
public:
    explicit Device(std::vector<RawFrame> script) : script_(std::move(script)) {}
    std::optional<RawFrame> read() {
        if (pos_ == script_.size()) return std::nullopt;
        return script_[pos_++];
    }

private:
    std::vector<RawFrame> script_;
    std::size_t pos_ = 0;
};

}  // namespace vendor_a

namespace vendor_b {

// HTTP-ish client: returns "ts_ms=<n>;motion=<0|1>" strings.
class Client {
public:
    explicit Client(std::vector<std::string> responses) : responses_(std::move(responses)) {}
    std::optional<std::string> fetch() {
        if (pos_ == responses_.size()) return std::nullopt;
        return responses_[pos_++];
    }

private:
    std::vector<std::string> responses_;
    std::size_t pos_ = 0;
};

}  // namespace vendor_b
