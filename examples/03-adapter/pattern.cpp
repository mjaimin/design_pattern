// 03 Adapter — object adapters (composition), one per vendor API.
//
// Force: third-party camera APIs differ in shape (HTTP text, C handles,
//        ONVIF profiles) and new vendors keep arriving; gateway code
//        (health sweep, recorder, UI) must see one Camera interface and
//        never include a vendor header.
//
// Roles
//   Target   : Camera                 — the interface the gateway wants
//   Adaptees : vendor_a::HttpClient, vendor_b C SDK, onvif::DeviceClient
//   Adapters : VendorACamera, VendorBCamera, OnvifCamera
//   Client   : sweep()                — knows only Camera
//
// Each adapter *translates*: names, data formats, error reporting (error
// codes → std::optional / bool), and resource rules (C handle → RAII).
// It adds no behaviour of its own.

#include <memory>
#include <optional>

#include "check.hpp"
#include "inventory.hpp"
#include "vendor_libs.hpp"

// ---- Target ------------------------------------------------------------------
class Camera {
public:
    virtual ~Camera() = default;
    virtual std::string model() const = 0;
    virtual std::optional<std::string> stream_url() const = 0;  // nullopt = unreachable
    virtual bool reboot() = 0;
};

// ---- Adapter: HTTP/CGI text API -------------------------------------------------
class VendorACamera final : public Camera {
public:
    explicit VendorACamera(vendor_a::HttpClient client) : http_(std::move(client)) {}

    std::string model() const override { return info("model"); }
    std::optional<std::string> stream_url() const override {
        return "rtsp://" + http_.ip() + ":554" + info("rtsp_path");
    }
    bool reboot() override { return http_.get("/cgi-bin/reboot") == "OK\n"; }

private:
    std::string info(const std::string& key) const {  // "key=value\n" → value
        const auto body = http_.get("/cgi-bin/info");
        const auto pos = body.find(key + "=");
        if (pos == std::string::npos) return "";
        const auto start = pos + key.size() + 1;
        return body.substr(start, body.find('\n', start) - start);
    }
    vendor_a::HttpClient http_;
};

// ---- Adapter: C SDK with handles and error codes ---------------------------------
// The handle is opened once and closed in the destructor: the adapter turns
// the SDK's manual resource rule into C++ RAII, so no call path can leak it.
class VendorBCamera final : public Camera {
public:
    VendorBCamera(const std::string& host, int channel) : channel_(channel) {
        online_ = vendor_b::vb_open(host.c_str(), &handle_) == vendor_b::VB_OK;
    }
    ~VendorBCamera() override {
        if (online_) vendor_b::vb_close(handle_);
    }
    VendorBCamera(const VendorBCamera&) = delete;
    VendorBCamera& operator=(const VendorBCamera&) = delete;

    std::string model() const override {
        char buf[64];
        if (!online_ || vendor_b::vb_get_model(handle_, buf, sizeof buf) != vendor_b::VB_OK) return "";
        return buf;
    }
    std::optional<std::string> stream_url() const override {
        char buf[128];
        if (!online_ || vendor_b::vb_get_stream_uri(handle_, channel_, buf, sizeof buf) != vendor_b::VB_OK)
            return std::nullopt;
        return std::string(buf);
    }
    bool reboot() override { return online_ && vendor_b::vb_reboot(handle_) == vendor_b::VB_OK; }

private:
    vendor_b::vb_handle handle_ = -1;
    int channel_;
    bool online_ = false;
};

// ---- Adapter: ONVIF profiles ---------------------------------------------------
class OnvifCamera final : public Camera {
public:
    explicit OnvifCamera(onvif::DeviceClient dev) : dev_(std::move(dev)) {
        for (const auto& p : dev_.get_profiles())  // the gateway wants H.264
            if (p.encoding == "H264") profile_token_ = p.token;
    }
    std::string model() const override { return dev_.get_device_information_model(); }
    std::optional<std::string> stream_url() const override {
        if (!profile_token_) return std::nullopt;
        return dev_.get_stream_uri(*profile_token_);
    }
    bool reboot() override {
        dev_.system_reboot();
        return true;
    }

private:
    onvif::DeviceClient dev_;
    std::optional<std::string> profile_token_;
};

// The only place that knows vendor names. (A simple factory, not the point here.)
std::unique_ptr<Camera> make_camera(const CameraConfig& c) {
    if (c.vendor == "vendor_a") return std::make_unique<VendorACamera>(vendor_a::HttpClient(c.host));
    if (c.vendor == "vendor_b") return std::make_unique<VendorBCamera>(c.host, c.channel);
    if (c.vendor == "onvif") return std::make_unique<OnvifCamera>(onvif::DeviceClient(c.host));
    return nullptr;
}

// ---- Client: knows only Camera -----------------------------------------------------
struct SweepResult {
    std::vector<std::string> urls, models;
    int online = 0, rebooted = 0;
};

SweepResult sweep(const std::vector<std::unique_ptr<Camera>>& cams) {
    SweepResult r;
    for (const auto& cam : cams) {
        r.models.push_back(cam->model());
        if (auto url = cam->stream_url()) {
            r.urls.push_back(*url);
            ++r.online;
        }
        if (cam->reboot()) ++r.rebooted;
    }
    return r;
}

int main() {
    SweepResult r;
    {
        std::vector<std::unique_ptr<Camera>> cams;
        for (const auto& cfg : inventory()) cams.push_back(make_camera(cfg));
        r = sweep(cams);
        CHECK_EQ(vendor_b::open_handle_count, 1);  // one live handle per online vendor_b camera
    }

    // Same results as naive.cpp.
    CHECK(r.urls == kExpectedUrls);
    CHECK(r.models == kExpectedModels);
    CHECK_EQ(r.online, kOnline);
    CHECK_EQ(r.rebooted, kRebooted);
    CHECK_EQ(vendor_b::open_handle_count, 0);  // adapters closed their handles

    std::cout << "03-adapter/pattern: online=" << r.online << " rebooted=" << r.rebooted
              << " leaked_handles=" << vendor_b::open_handle_count << "\n";
}
