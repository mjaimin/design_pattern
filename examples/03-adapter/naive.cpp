// 03 Adapter — the naive version.
//
// The gateway talks to each vendor API directly. Every operation the gateway
// needs (model, stream URL, reboot) is a function with an if/else over the
// vendor, and each branch speaks that vendor's dialect: parse key=value text,
// juggle C handles and error codes, pick an ONVIF profile.
//
// Where it hurts — try adding a fourth vendor, or a fourth operation:
//   - new vendor: edit *every* operation function (Shotgun Surgery)
//   - new operation: write the three dialects again, inline
//   - the vendor_b handle open/close is repeated per call; forget one
//     vb_close() on an early return and the SDK leaks a handle
//   - every caller of these functions includes every vendor SDK header
// Gateway logic and vendor protocol detail are tangled in the same lines.

#include <optional>

#include "check.hpp"
#include "inventory.hpp"
#include "vendor_libs.hpp"

std::string kv_lookup(const std::string& body, const std::string& key) {
    const auto pos = body.find(key + "=");
    if (pos == std::string::npos) return "";
    const auto start = pos + key.size() + 1;
    return body.substr(start, body.find('\n', start) - start);
}

std::string model_of(const CameraConfig& c) {
    if (c.vendor == "vendor_a") {
        return kv_lookup(vendor_a::HttpClient(c.host).get("/cgi-bin/info"), "model");
    } else if (c.vendor == "vendor_b") {
        vendor_b::vb_handle h;
        if (vendor_b::vb_open(c.host.c_str(), &h) != vendor_b::VB_OK) return "";
        char buf[64];
        const int rc = vendor_b::vb_get_model(h, buf, sizeof buf);
        vendor_b::vb_close(h);
        return rc == vendor_b::VB_OK ? buf : "";
    } else if (c.vendor == "onvif") {
        return onvif::DeviceClient(c.host).get_device_information_model();
    }
    return "";
}

std::optional<std::string> stream_url_of(const CameraConfig& c) {
    if (c.vendor == "vendor_a") {
        const auto path = kv_lookup(vendor_a::HttpClient(c.host).get("/cgi-bin/info"), "rtsp_path");
        return "rtsp://" + c.host + ":554" + path;
    } else if (c.vendor == "vendor_b") {
        vendor_b::vb_handle h;
        if (vendor_b::vb_open(c.host.c_str(), &h) != vendor_b::VB_OK) return std::nullopt;
        char buf[128];
        const int rc = vendor_b::vb_get_stream_uri(h, c.channel, buf, sizeof buf);
        vendor_b::vb_close(h);
        if (rc != vendor_b::VB_OK) return std::nullopt;
        return std::string(buf);
    } else if (c.vendor == "onvif") {
        onvif::DeviceClient dev(c.host);
        for (const auto& p : dev.get_profiles())
            if (p.encoding == "H264") return dev.get_stream_uri(p.token);
        return std::nullopt;
    }
    return std::nullopt;
}

bool reboot(const CameraConfig& c) {
    if (c.vendor == "vendor_a") {
        return vendor_a::HttpClient(c.host).get("/cgi-bin/reboot") == "OK\n";
    } else if (c.vendor == "vendor_b") {
        vendor_b::vb_handle h;
        if (vendor_b::vb_open(c.host.c_str(), &h) != vendor_b::VB_OK) return false;
        const int rc = vendor_b::vb_reboot(h);
        vendor_b::vb_close(h);
        return rc == vendor_b::VB_OK;
    } else if (c.vendor == "onvif") {
        onvif::DeviceClient(c.host).system_reboot();
        return true;
    }
    return false;
}

int main() {
    std::vector<std::string> urls, models;
    int online = 0, rebooted = 0;
    for (const auto& cfg : inventory()) {
        models.push_back(model_of(cfg));
        if (auto url = stream_url_of(cfg)) {
            urls.push_back(*url);
            ++online;
        }
        if (reboot(cfg)) ++rebooted;
    }

    CHECK(urls == kExpectedUrls);
    CHECK(models == kExpectedModels);
    CHECK_EQ(online, kOnline);
    CHECK_EQ(rebooted, kRebooted);
    CHECK_EQ(vendor_b::open_handle_count, 0);

    std::cout << "03-adapter/naive: online=" << online << " rebooted=" << rebooted
              << " leaked_handles=" << vendor_b::open_handle_count << "\n";
}
