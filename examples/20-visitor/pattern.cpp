// 20 Visitor — classic form (double dispatch).
//
// Force: the set of device *types* is stable; the set of *operations* over
//        the device tree keeps growing. New operations must not edit the
//        device classes, and adding a device type must force every operation
//        to handle it.
//
// Roles
//   Visitor          : DeviceVisitor — one visit() overload per element type
//   Concrete visitors : LicenceCounter, HealthReport, CsvExporter — one operation each
//   Element          : Device        — accept(visitor)
//   Concrete elements : Gateway, Camera, Relay, Sensor
//   Object structure : the gateway tree (Gateway::accept walks its children)
//
// Double dispatch: device->accept(v) picks the *element* type (virtual call
// #1); inside, v.visit(*this) picks the overload by the static type of *this
// and the *visitor* type (virtual call #2). Two runtime types → one function.

#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "expected.hpp"

class Gateway;
class Camera;
class Relay;
class Sensor;

// ---- Visitor --------------------------------------------------------------
// Adding an element type adds a pure virtual here → every visitor fails to
// compile until it handles the new type. That is the safety the naive
// version lacked.
class DeviceVisitor {
public:
    virtual ~DeviceVisitor() = default;
    virtual void visit(const Gateway&) = 0;
    virtual void visit(const Camera&) = 0;
    virtual void visit(const Relay&) = 0;
    virtual void visit(const Sensor&) = 0;
};

// ---- Elements -------------------------------------------------------------
class Device {
public:
    Device(std::string id, bool online) : id_(std::move(id)), online_(online) {}
    virtual ~Device() = default;
    virtual void accept(DeviceVisitor& v) const = 0;
    const std::string& id() const { return id_; }
    bool online() const { return online_; }

private:
    std::string id_;
    bool online_;
};

class Camera final : public Device {
public:
    Camera(std::string id, bool online, bool ai) : Device(std::move(id), online), ai_(ai) {}
    bool ai_enabled() const { return ai_; }
    void accept(DeviceVisitor& v) const override { v.visit(*this); }

private:
    bool ai_;
};

class Relay final : public Device {
public:
    using Device::Device;
    void accept(DeviceVisitor& v) const override { v.visit(*this); }
};

class Sensor final : public Device {
public:
    using Device::Device;
    void accept(DeviceVisitor& v) const override { v.visit(*this); }
};

class Gateway final : public Device {
public:
    using Device::Device;
    void add(std::unique_ptr<Device> d) { children_.push_back(std::move(d)); }

    // The structure owns the traversal: visit me, then my children (pre-order).
    void accept(DeviceVisitor& v) const override {
        v.visit(*this);
        for (const auto& c : children_) c->accept(v);
    }

private:
    std::vector<std::unique_ptr<Device>> children_;
};

// ---- Concrete visitors: one class = one operation ---------------------------
class LicenceCounter final : public DeviceVisitor {
public:
    void visit(const Gateway&) override {}
    void visit(const Camera& c) override { total += c.ai_enabled() ? 2 : 1; }
    void visit(const Relay&) override {}
    void visit(const Sensor&) override {}
    int total = 0;
};

class HealthReport final : public DeviceVisitor {
public:
    void visit(const Gateway& g) override { check(g); }
    void visit(const Camera& c) override { check(c); }
    void visit(const Relay& r) override { check(r); }
    void visit(const Sensor& s) override { check(s); }
    std::vector<std::string> offline;

private:
    void check(const Device& d) {
        if (!d.online()) offline.push_back(d.id());
    }
};

class CsvExporter final : public DeviceVisitor {
public:
    void visit(const Gateway& g) override { row("gateway", g); }
    void visit(const Camera& c) override { row("camera", c); }
    void visit(const Relay& r) override { row("relay", r); }
    void visit(const Sensor& s) override { row("sensor", s); }
    std::string out;

private:
    void row(const char* kind, const Device& d) {
        out += std::string(kind) + "," + d.id() + "," + (d.online() ? "online" : "offline") + "\n";
    }
};

std::vector<std::unique_ptr<Gateway>> build_site() {
    std::vector<std::unique_ptr<Gateway>> site;
    auto g1 = std::make_unique<Gateway>("gw-1", true);
    g1->add(std::make_unique<Camera>("cam-a", true, true));
    g1->add(std::make_unique<Camera>("cam-b", false, false));
    g1->add(std::make_unique<Relay>("r1", true));
    g1->add(std::make_unique<Sensor>("s1", false));
    auto g2 = std::make_unique<Gateway>("gw-2", true);
    g2->add(std::make_unique<Camera>("cam-c", true, false));
    g2->add(std::make_unique<Relay>("r2", true));
    site.push_back(std::move(g1));
    site.push_back(std::move(g2));
    return site;
}

int main() {
    const auto site = build_site();

    LicenceCounter lic;
    HealthReport health;
    CsvExporter csv;
    for (const auto& g : site) {
        g->accept(lic);
        g->accept(health);
        g->accept(csv);
    }

    // Same numbers as naive.cpp.
    CHECK_EQ(lic.total, kLicences);
    CHECK(health.offline == kOffline);
    CHECK_EQ(csv.out, kCsv);

    std::cout << "20-visitor/pattern: licences=" << lic.total << " offline=" << health.offline.size()
              << " csv_rows=8\n";
}
