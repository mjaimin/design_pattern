// 21 Prototype — classic form: virtual clone().
//
// Force: the *kind* of camera being duplicated varies (dome, bullet, PTZ...),
//        and so does its configured state; the onboarding workflow must
//        stamp out copies of whatever template it is handed without knowing
//        its concrete class.
//
// Roles
//   Prototype          : Camera              — declares clone()
//   Concrete prototypes : DomeCamera, BulletCamera — each copies itself
//   Client             : onboard()           — clones, then customises (address)
//   Prototype registry : TemplateRegistry    — named, pre-configured templates
//
// Compare with naive.cpp: copy_of() and its dynamic_cast chain are gone.
// The object that knows its own concrete type does the copying.

#include <map>
#include <set>

#include "check.hpp"
#include "fleet.hpp"

class Camera {
public:
    virtual ~Camera() = default;
    virtual std::string model() const = 0;

    // The virtual constructor. Returns unique_ptr<Camera>, not
    // unique_ptr<DomeCamera>: covariant return types work only for raw
    // pointers and references, not smart pointers (see modern.cpp).
    virtual std::unique_ptr<Camera> clone() const = 0;

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

    // Same deep/shared copy rules as naive.cpp — clone() relies on them.
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
    std::unique_ptr<Camera> clone() const override { return std::make_unique<DomeCamera>(*this); }
    int ir_range_m() const { return ir_range_m_; }

private:
    int ir_range_m_;
};

class BulletCamera final : public Camera {
public:
    BulletCamera(std::shared_ptr<const OsdFont> font, int lens_mm)
        : Camera(std::move(font)), lens_mm_(lens_mm) {}
    std::string model() const override { return "bullet"; }
    std::unique_ptr<Camera> clone() const override { return std::make_unique<BulletCamera>(*this); }
    int lens_mm() const { return lens_mm_; }

private:
    int lens_mm_;
};

// ---- Prototype registry ------------------------------------------------------
// Installers save templates by name; onboarding asks for a copy by name.
class TemplateRegistry {
public:
    void save(const std::string& name, std::unique_ptr<Camera> tmpl) {
        templates_[name] = std::move(tmpl);
    }
    std::unique_ptr<Camera> make(const std::string& name) const { return templates_.at(name)->clone(); }
    const Camera& get(const std::string& name) const { return *templates_.at(name); }

private:
    std::map<std::string, std::unique_ptr<Camera>> templates_;
};

// ---- Client: knows only Camera and the registry ------------------------------
std::unique_ptr<Camera> onboard(const TemplateRegistry& reg, const std::string& tmpl, std::string ip) {
    auto cam = reg.make(tmpl);
    cam->set_address(std::move(ip));
    return cam;
}

int main() {
    auto font = std::make_shared<const OsdFont>(OsdFont{"mono", std::vector<unsigned char>(64 * 1024)});

    auto lobby = std::make_unique<DomeCamera>(font, 15);
    lobby->add_zone({"door", {{0, 0}, {100, 0}, {100, 200}}});
    lobby->add_zone({"desk-mask", {{300, 300}, {400, 300}, {400, 400}}});
    lobby->motion().sensitivity = kTemplateSensitivity;

    auto parking = std::make_unique<BulletCamera>(font, 12);
    parking->add_zone({"gate", {{0, 0}, {50, 50}, {0, 50}}});
    parking->add_zone({"plate-area", {{10, 10}, {20, 20}, {10, 20}}});
    parking->motion().sensitivity = kTemplateSensitivity;

    TemplateRegistry reg;
    reg.save("lobby-dome", std::move(lobby));
    reg.save("parking-bullet", std::move(parking));

    std::vector<std::unique_ptr<Camera>> fleet;
    for (int i = 1; i <= kDomes; ++i) fleet.push_back(onboard(reg, "lobby-dome", "10.1.0." + std::to_string(i)));
    for (int i = 1; i <= kBullets; ++i) fleet.push_back(onboard(reg, "parking-bullet", "10.2.0." + std::to_string(i)));

    // Same checks as naive.cpp → same numbers.
    std::set<std::string> addresses;
    int domes = 0;
    for (const auto& c : fleet) {
        addresses.insert(c->address());
        if (c->model() == "dome") ++domes;
        CHECK_EQ(c->zones().size(), kZonesPerTemplate);
        CHECK(c->font() == font);
    }
    CHECK_EQ(fleet.size(), kDomes + kBullets);
    CHECK_EQ(domes, kDomes);
    CHECK_EQ(addresses.size(), kDomes + kBullets);

    fleet[0]->motion().sensitivity = 10;
    CHECK_EQ(reg.get("lobby-dome").motion().sensitivity, kTemplateSensitivity);
    CHECK_EQ(fleet[1]->motion().sensitivity, kTemplateSensitivity);

    std::cout << "21-prototype/pattern: onboarded=" << fleet.size() << " domes=" << domes
              << " bullets=" << fleet.size() - domes << " font_refs=" << font.use_count() << "\n";
}
