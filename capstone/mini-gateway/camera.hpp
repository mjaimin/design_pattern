#pragma once
// ADAPTER (ch. 03): two incompatible vendor APIs → one Camera interface.
// Force: vendor APIs vary and are not ours to change; everything downstream
// of "a frame arrived" must not know which vendor produced it.

#include <memory>
#include <optional>
#include <string>

#include "domain.hpp"
#include "vendors.hpp"

namespace mg {

class Camera {
public:
    virtual ~Camera() = default;
    virtual const gw::CameraId& id() const = 0;
    virtual std::optional<gw::Frame> next_frame() = 0;  // nullopt = stream ended
};

class VendorACamera final : public Camera {
public:
    VendorACamera(gw::CameraId id, vendor_a::Device dev) : id_(std::move(id)), dev_(std::move(dev)) {}
    const gw::CameraId& id() const override { return id_; }
    std::optional<gw::Frame> next_frame() override {
        auto raw = dev_.read();
        if (!raw) return std::nullopt;
        return gw::Frame{id_, raw->pts_us / 1000, raw->md == 1};  // µs → ms, byte → bool
    }

private:
    gw::CameraId id_;
    vendor_a::Device dev_;
};

class VendorBCamera final : public Camera {
public:
    VendorBCamera(gw::CameraId id, vendor_b::Client client)
        : id_(std::move(id)), client_(std::move(client)) {}
    const gw::CameraId& id() const override { return id_; }
    std::optional<gw::Frame> next_frame() override {
        auto body = client_.fetch();
        if (!body) return std::nullopt;
        // "ts_ms=<n>;motion=<0|1>"
        const auto semi = body->find(';');
        const auto ts = std::stoll(body->substr(6, semi - 6));
        const bool motion = body->back() == '1';
        return gw::Frame{id_, ts, motion};
    }

private:
    gw::CameraId id_;
    vendor_b::Client client_;
};

}  // namespace mg
