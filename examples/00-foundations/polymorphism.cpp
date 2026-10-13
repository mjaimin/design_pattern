// 00 Foundations — the four ways C++ lets one call site work with many types.
//
// Same job four times: "give me the RTSP URL of this camera", where the URL
// layout differs per vendor. Every GoF pattern is built on one of these four
// mechanisms, so knowing their trade-offs is knowing which C++ form a pattern
// should take.
//
//   1. virtual functions      runtime  · open set of types  · via pointer/reference
//   2. templates + concepts   compile time · open set       · no common base at all
//   3. std::function          runtime  · open set           · value semantics, type erased
//   4. std::variant + visit   runtime  · CLOSED set         · value semantics, no heap

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "check.hpp"

// ---------------------------------------------------------------------------
// 1. Subtype polymorphism: an abstract base and virtual dispatch.
//    Callers know only `Camera`; new vendors are new subclasses.
// ---------------------------------------------------------------------------
namespace virtual_form {

class Camera {
public:
    virtual ~Camera() = default;
    virtual std::string rtsp_url() const = 0;
};

class VendorACamera final : public Camera {
public:
    explicit VendorACamera(std::string host) : host_(std::move(host)) {}
    std::string rtsp_url() const override { return "rtsp://" + host_ + ":554/stream1"; }

private:
    std::string host_;
};

class VendorBCamera final : public Camera {
public:
    VendorBCamera(std::string host, int channel) : host_(std::move(host)), channel_(channel) {}
    std::string rtsp_url() const override {
        return "rtsp://" + host_ + ":554/h264/ch" + std::to_string(channel_) + "/main";
    }

private:
    std::string host_;
    int channel_;
};

// The call site: one loop, any mix of vendors, chosen at runtime.
std::vector<std::string> urls(const std::vector<std::unique_ptr<Camera>>& cameras) {
    std::vector<std::string> out;
    for (const auto& cam : cameras) out.push_back(cam->rtsp_url());
    return out;
}

}  // namespace virtual_form

// ---------------------------------------------------------------------------
// Forms 2-4 share these plain structs: no base class, no virtual, no heap.
// ---------------------------------------------------------------------------
struct VendorACam {
    std::string host;
    std::string rtsp_url() const { return "rtsp://" + host + ":554/stream1"; }
};

struct VendorBCam {
    std::string host;
    int channel;
    std::string rtsp_url() const {
        return "rtsp://" + host + ":554/h264/ch" + std::to_string(channel) + "/main";
    }
};

// ---------------------------------------------------------------------------
// 2. Parametric (static) polymorphism: a template constrained by a concept.
//    Resolved at compile time — zero dispatch cost, fully inlinable — but a
//    std::vector cannot hold a mix of VendorACam and VendorBCam this way.
// ---------------------------------------------------------------------------
namespace template_form {

template <class C>
concept HasRtspUrl = requires(const C& c) {
    { c.rtsp_url() } -> std::convertible_to<std::string>;
};

template <HasRtspUrl C>
std::string url_of(const C& cam) {
    return cam.rtsp_url();
}

}  // namespace template_form

// ---------------------------------------------------------------------------
// 3. Type erasure via std::function: any callable with the right signature.
//    The heterogeneous vector is back, without a base class — the erasure
//    hides the concrete type behind a uniform call.
// ---------------------------------------------------------------------------
namespace function_form {

using UrlSource = std::function<std::string()>;

std::vector<std::string> urls(const std::vector<UrlSource>& sources) {
    std::vector<std::string> out;
    for (const auto& src : sources) out.push_back(src());
    return out;
}

}  // namespace function_form

// ---------------------------------------------------------------------------
// 4. Closed sum type: std::variant + std::visit.
//    The set of vendors is fixed in the type; the compiler checks every
//    visitor handles every alternative. Adding a vendor means editing the
//    variant — the trade-off you accept for exhaustiveness.
// ---------------------------------------------------------------------------
namespace variant_form {

using AnyCamera = std::variant<VendorACam, VendorBCam>;

std::vector<std::string> urls(const std::vector<AnyCamera>& cameras) {
    std::vector<std::string> out;
    for (const auto& cam : cameras)
        out.push_back(std::visit([](const auto& c) { return c.rtsp_url(); }, cam));
    return out;
}

}  // namespace variant_form

int main() {
    const std::vector<std::string> expected = {
        "rtsp://10.0.0.11:554/stream1",
        "rtsp://10.0.0.12:554/h264/ch2/main",
    };

    // 1. virtual
    std::vector<std::unique_ptr<virtual_form::Camera>> cams;
    cams.push_back(std::make_unique<virtual_form::VendorACamera>("10.0.0.11"));
    cams.push_back(std::make_unique<virtual_form::VendorBCamera>("10.0.0.12", 2));
    CHECK(virtual_form::urls(cams) == expected);

    // 2. template — one instantiation per type, no shared container
    CHECK_EQ(template_form::url_of(VendorACam{"10.0.0.11"}), expected[0]);
    CHECK_EQ(template_form::url_of(VendorBCam{"10.0.0.12", 2}), expected[1]);
    static_assert(!template_form::HasRtspUrl<int>);  // rejected at compile time

    // 3. std::function
    std::vector<function_form::UrlSource> sources = {
        [c = VendorACam{"10.0.0.11"}] { return c.rtsp_url(); },
        [c = VendorBCam{"10.0.0.12", 2}] { return c.rtsp_url(); },
    };
    CHECK(function_form::urls(sources) == expected);

    // 4. variant
    std::vector<variant_form::AnyCamera> any = {VendorACam{"10.0.0.11"},
                                                VendorBCam{"10.0.0.12", 2}};
    CHECK(variant_form::urls(any) == expected);

    std::cout << "00-foundations/polymorphism: all four forms agree\n";
}
