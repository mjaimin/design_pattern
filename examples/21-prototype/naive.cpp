// 21 Prototype — the naive version.
//
// An installer configures one camera per model by hand (zones, motion grid,
// OSD font, IR range...) and wants 50 more just like it. The onboarding code
// only has a `const Camera&` to the template — it does not know the model.
//
// C++ cannot copy through a base reference: `Camera copy = tmpl;` would
// *slice* (keep only the base part) — which is why Camera's copy constructor
// is protected here. So the onboarding code asks "which model are you?" with
// dynamic_cast and calls the right copy constructor itself.
//
// Where it hurts — add a `PtzCamera` model:
//   - copy_of() needs a new branch; forget it and onboarding throws at
//     runtime, on site, the day an installer first uses a PTZ template;
//   - onboarding (a generic workflow) now depends on every concrete model.
// Smells: type tests on a hierarchy (Switch Statements), and a generic module
// coupled to every concrete class (dependency on details, not abstractions).

#include <set>
#include <stdexcept>

#include "check.hpp"
#include "fleet.hpp"

class Camera {
public:
    virtual ~Camera() = default;
    virtual std::string model() const = 0;

    void set_address(std::string ip) { address_ = std::move(ip); }
    const std::string& address() const { return address_; }
    void add_zone(Zone z) { zones_.push_back(std::move(z)); }
    const std::vector<Zone>& zones() const { return zones_; }
    MotionGrid& motion() { return *motion_; }
    const MotionGrid& motion() const { return *motion_; }
    const std::shared_ptr<const OsdFont>& font() const { return font_; }

protected:
    explicit Camera(std::shared_ptr<const OsdFont> font)
        : motion_(std::make_unique<MotionGrid>()), font_(std::move(font)) {}

    // Protected: only derived classes may copy, so a Camera can't be sliced.
    // Hand-written because unique_ptr is move-only: deep-copy the grid,
    // share the immutable font.
    Camera(const Camera& o)
        : address_(o.address_),
          zones_(o.zones_),
          motion_(std::make_unique<MotionGrid>(*o.motion_)),
          font_(o.font_) {}
    Camera& operator=(const Camera&) = delete;

private:
    std::string address_;
    std::vector<Zone> zones_;
    std::unique_ptr<MotionGrid> motion_;
    std::shared_ptr<const OsdFont> font_;
};

class DomeCamera final : public Camera {
public:
    DomeCamera(std::shared_ptr<const OsdFont> font, int ir_range_m)
        : Camera(std::move(font)), ir_range_m_(ir_range_m) {}
    std::string model() const override { return "dome"; }
    int ir_range_m() const { return ir_range_m_; }

private:
    int ir_range_m_;
};

class BulletCamera final : public Camera {
public:
    BulletCamera(std::shared_ptr<const OsdFont> font, int lens_mm)
        : Camera(std::move(font)), lens_mm_(lens_mm) {}
    std::string model() const override { return "bullet"; }
    int lens_mm() const { return lens_mm_; }

private:
    int lens_mm_;
};

// The generic workflow that must know every concrete model.
std::unique_ptr<Camera> copy_of(const Camera& tmpl) {
    if (auto* d = dynamic_cast<const DomeCamera*>(&tmpl)) return std::make_unique<DomeCamera>(*d);
    if (auto* b = dynamic_cast<const BulletCamera*>(&tmpl)) return std::make_unique<BulletCamera>(*b);
    throw std::logic_error("onboarding: unknown camera model " + tmpl.model());
}

std::unique_ptr<Camera> onboard(const Camera& tmpl, std::string ip) {
    auto cam = copy_of(tmpl);
    cam->set_address(std::move(ip));
    return cam;
}

int main() {
    auto font = std::make_shared<const OsdFont>(OsdFont{"mono", std::vector<unsigned char>(64 * 1024)});

    DomeCamera lobby(font, 15);
    lobby.add_zone({"door", {{0, 0}, {100, 0}, {100, 200}}});
    lobby.add_zone({"desk-mask", {{300, 300}, {400, 300}, {400, 400}}});
    lobby.motion().sensitivity = kTemplateSensitivity;

    BulletCamera parking(font, 12);
    parking.add_zone({"gate", {{0, 0}, {50, 50}, {0, 50}}});
    parking.add_zone({"plate-area", {{10, 10}, {20, 20}, {10, 20}}});
    parking.motion().sensitivity = kTemplateSensitivity;

    std::vector<std::unique_ptr<Camera>> fleet;
    for (int i = 1; i <= kDomes; ++i) fleet.push_back(onboard(lobby, "10.1.0." + std::to_string(i)));
    for (int i = 1; i <= kBullets; ++i) fleet.push_back(onboard(parking, "10.2.0." + std::to_string(i)));

    std::set<std::string> addresses;
    int domes = 0;
    for (const auto& c : fleet) {
        addresses.insert(c->address());
        if (c->model() == "dome") ++domes;
        CHECK_EQ(c->zones().size(), kZonesPerTemplate);
        CHECK(c->font() == font);  // shared, not copied
    }
    CHECK_EQ(fleet.size(), kDomes + kBullets);
    CHECK_EQ(domes, kDomes);
    CHECK_EQ(addresses.size(), kDomes + kBullets);

    // Deep copy: tuning one clone must not touch the template or its siblings.
    fleet[0]->motion().sensitivity = 10;
    CHECK_EQ(lobby.motion().sensitivity, kTemplateSensitivity);
    CHECK_EQ(fleet[1]->motion().sensitivity, kTemplateSensitivity);

    std::cout << "21-prototype/naive: onboarded=" << fleet.size() << " domes=" << domes
              << " bullets=" << fleet.size() - domes << " font_refs=" << font.use_count() << "\n";
}
