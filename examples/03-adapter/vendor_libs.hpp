#pragma once
// Simulated third-party camera APIs. Treat this file as code you do NOT own:
// you can't change these signatures, only call them. Each one models a style
// real camera integrations come in:
//   vendor_a — HTTP/CGI style: GET a path, parse "key=value" lines
//   vendor_b — C SDK style: integer handles, out-buffers, negative error codes
//   onvif    — standard-protocol style: media profiles, pick one, ask for its URI

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
namespace vendor_a {

class HttpClient {
public:
    explicit HttpClient(std::string ip) : ip_(std::move(ip)) {}

    // Returns the response body, or "" on 404.
    std::string get(const std::string& path) const {
        if (path == "/cgi-bin/info") return "model=A-200\nfirmware=1.4.2\nrtsp_path=/stream1\n";
        if (path == "/cgi-bin/reboot") return "OK\n";
        return "";
    }
    const std::string& ip() const { return ip_; }

private:
    std::string ip_;
};

}  // namespace vendor_a

// ---------------------------------------------------------------------------
namespace vendor_b {

using vb_handle = int;
inline constexpr int VB_OK = 0;
inline constexpr int VB_ERR_OFFLINE = -2;
inline constexpr int VB_ERR_BAD_HANDLE = -3;

namespace detail {
inline std::vector<std::string>& open_hosts() {
    static std::vector<std::string> hosts;
    return hosts;
}
inline void copy_out(const std::string& s, char* buf, std::size_t len) {
    const std::size_t n = std::min(len - 1, s.size());
    std::memcpy(buf, s.data(), n);
    buf[n] = '\0';
}
}  // namespace detail

// Not part of a real SDK: a counter that lets the examples prove every
// handle they open is closed again.
inline int open_handle_count = 0;

// The SDK treats 10.0.0.99 as an unreachable device.
inline int vb_open(const char* host, vb_handle* out) {
    if (std::string(host) == "10.0.0.99") return VB_ERR_OFFLINE;
    ++open_handle_count;
    detail::open_hosts().push_back(host);
    *out = static_cast<vb_handle>(detail::open_hosts().size() - 1);
    return VB_OK;
}
inline int vb_get_model(vb_handle h, char* buf, std::size_t len) {
    if (h < 0 || h >= static_cast<int>(detail::open_hosts().size())) return VB_ERR_BAD_HANDLE;
    detail::copy_out("B-NVR-8", buf, len);
    return VB_OK;
}
inline int vb_get_stream_uri(vb_handle h, int channel, char* buf, std::size_t len) {
    if (h < 0 || h >= static_cast<int>(detail::open_hosts().size())) return VB_ERR_BAD_HANDLE;
    detail::copy_out("rtsp://" + detail::open_hosts()[static_cast<std::size_t>(h)] +
                         ":554/h264/ch" + std::to_string(channel) + "/main",
                     buf, len);
    return VB_OK;
}
inline int vb_reboot(vb_handle h) {
    return (h < 0 || h >= static_cast<int>(detail::open_hosts().size())) ? VB_ERR_BAD_HANDLE : VB_OK;
}
inline void vb_close(vb_handle) { --open_handle_count; }

}  // namespace vendor_b

// ---------------------------------------------------------------------------
namespace onvif {

struct Profile {
    std::string token;
    std::string encoding;  // "JPEG", "H264", "H265"
};

class DeviceClient {
public:
    explicit DeviceClient(std::string xaddr) : xaddr_(std::move(xaddr)) {}

    std::string get_device_information_model() const { return "ONVIF-Cam-X"; }
    std::vector<Profile> get_profiles() const {
        return {{"profile_jpeg", "JPEG"}, {"profile_h264", "H264"}};
    }
    std::string get_stream_uri(const std::string& profile_token) const {
        return "rtsp://" + xaddr_ + ":554/onvif/" + profile_token;
    }
    void system_reboot() { ++reboots; }

    int reboots = 0;

private:
    std::string xaddr_;
};

}  // namespace onvif
