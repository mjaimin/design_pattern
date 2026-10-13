// 21 Prototype — two modern C++ forms.
//
// A. CRTP clone mixin — fixes the covariance problem.
//    pattern.cpp's DomeCamera::clone() must return unique_ptr<Camera>, so code
//    that *has* a DomeCamera loses its static type on clone. C++ allows
//    covariant returns only for raw pointers/references. The fix (NVI + CRTP):
//      - Camera::clone()        public, non-virtual → unique_ptr<Camera>
//      - Camera::clone_impl()   private virtual     → Camera* (raw, owned at once)
//      - Cloneable<D, B>::clone() hides it          → unique_ptr<D>
//      - Cloneable<D, B>::clone_impl() written once for every model
//    No model class writes clone() by hand any more.
//
// B. Value types — the copy constructor *is* the prototype.
//    If a camera template is a regular value type (or a std::variant of value
//    types), `auto cam = tmpl;` is a polymorphic deep copy. No hierarchy, no
//    clone(). Prefer this whenever the models don't need to be an open set.

#include <memory>
#include <set>
#include <variant>

#include "check.hpp"
#include "fleet.hpp"

// ---------------------------------------------------------------------------
// A. CRTP clone mixin
// ---------------------------------------------------------------------------
namespace crtp_form {

class Camera {
public:
    virtual ~Camera() = default;
    virtual std::string model() const = 0;
    std::unique_ptr<Camera> clone() const { return std::unique_ptr<Camera>(clone_impl()); }

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
    Camera(const Camera& o)
        : address_(o.address_),
          zones_(o.zones_),
          motion_(std::make_unique<MotionGrid>(*o.motion_)),
          font_(o.font_) {}
    Camera& operator=(const Camera&) = delete;

private:
    virtual Camera* clone_impl() const = 0;

    std::string address_;
    std::vector<Zone> zones_;
    std::unique_ptr<MotionGrid> motion_;
    std::shared_ptr<const OsdFont> font_;
};

template <class Derived, class Base>
class Cloneable : public Base {
public:
    using Base::Base;
    std::unique_ptr<Derived> clone() const {  // hides Base::clone, keeps the static type
        return std::unique_ptr<Derived>(static_cast<Derived*>(clone_impl()));
    }

private:
    Base* clone_impl() const override { return new Derived(static_cast<const Derived&>(*this)); }
};

class DomeCamera final : public Cloneable<DomeCamera, Camera> {
public:
    DomeCamera(std::shared_ptr<const OsdFont> font, int ir_range_m)
        : Cloneable(std::move(font)), ir_range_m_(ir_range_m) {}
    std::string model() const override { return "dome"; }
    int ir_range_m() const { return ir_range_m_; }

private:
    int ir_range_m_;
};

class BulletCamera final : public Cloneable<BulletCamera, Camera> {
public:
    BulletCamera(std::shared_ptr<const OsdFont> font, int lens_mm)
        : Cloneable(std::move(font)), lens_mm_(lens_mm) {}
    std::string model() const override { return "bullet"; }
    int lens_mm() const { return lens_mm_; }

private:
    int lens_mm_;
};

}  // namespace crtp_form

// ---------------------------------------------------------------------------
// B. Value types + std::variant
// ---------------------------------------------------------------------------
namespace value_form {

struct Common {
    std::string address;
    std::vector<Zone> zones;
    MotionGrid motion;                     // by value: copying is deep automatically
    std::shared_ptr<const OsdFont> font;   // shared on purpose
};
struct Dome {
    Common c;
    int ir_range_m;
};
struct Bullet {
    Common c;
    int lens_mm;
};
using CameraTemplate = std::variant<Dome, Bullet>;

Common& common(CameraTemplate& t) {
    return std::visit([](auto& x) -> Common& { return x.c; }, t);
}

CameraTemplate onboard(const CameraTemplate& tmpl, std::string ip) {
    CameraTemplate cam = tmpl;  // the whole "clone()" — variant copies the active model
    common(cam).address = std::move(ip);
    return cam;
}

}  // namespace value_form

int main() {
    auto font = std::make_shared<const OsdFont>(OsdFont{"mono", std::vector<unsigned char>(64 * 1024)});

    // ---- A ----
    {
        using namespace crtp_form;
        DomeCamera lobby(font, 15);
        lobby.add_zone({"door", {{0, 0}, {100, 0}, {100, 200}}});
        lobby.add_zone({"desk-mask", {{300, 300}, {400, 300}, {400, 400}}});
        lobby.motion().sensitivity = kTemplateSensitivity;

        std::unique_ptr<DomeCamera> typed = lobby.clone();  // static type kept
        CHECK_EQ(typed->ir_range_m(), 15);

        const Camera& as_base = lobby;
        std::unique_ptr<Camera> erased = as_base.clone();  // still a DomeCamera inside
        CHECK(erased->model() == "dome");

        std::set<std::string> addresses;
        for (int i = 1; i <= kDomes; ++i) {
            auto cam = as_base.clone();
            cam->set_address("10.1.0." + std::to_string(i));
            addresses.insert(cam->address());
            CHECK_EQ(cam->zones().size(), kZonesPerTemplate);
        }
        CHECK_EQ(addresses.size(), kDomes);
        typed->motion().sensitivity = 10;
        CHECK_EQ(lobby.motion().sensitivity, kTemplateSensitivity);
    }

    // ---- B ----
    {
        using namespace value_form;
        Bullet parking{{"", {{"gate", {{0, 0}, {50, 50}, {0, 50}}}, {"plate-area", {{10, 10}, {20, 20}, {10, 20}}}},
                        MotionGrid{{}, kTemplateSensitivity}, font},
                       12};
        const CameraTemplate tmpl = parking;

        std::vector<CameraTemplate> fleet;
        for (int i = 1; i <= kBullets; ++i) fleet.push_back(onboard(tmpl, "10.2.0." + std::to_string(i)));
        CHECK_EQ(fleet.size(), kBullets);
        CHECK(std::holds_alternative<Bullet>(fleet[0]));
        common(fleet[0]).motion.sensitivity = 10;
        CHECK_EQ(std::get<Bullet>(tmpl).c.motion.sensitivity, kTemplateSensitivity);
        CHECK(common(fleet[3]).font == font);
    }

    std::cout << "21-prototype/modern: CRTP clone keeps static type; value copy needs no clone()\n";
}
