// 11 Composite — classic form (one interface for leaves and groups).
//
// Force: the device tree's depth and shape vary per customer (site → zones,
//        or site → buildings → floors → zones), and operators act on any node:
//        "arm this", "how healthy is this". The operations and their callers
//        must not care whether the node is one device or a subtree.
//
// Roles
//   Component : Node    — what callers see: arm, counts, health, find
//   Leaf      : Device  — a camera or relay; answers for itself
//   Composite : Group   — a site, building, floor or zone; answers by asking
//                         its children and combining the results
//
// Compare with naive.cpp: one implementation per operation per *role*
// (leaf / group), not per level. Depth is no longer part of the type system.

#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "fixture.hpp"

// ---- Component ---------------------------------------------------------------
class Node {
public:
    explicit Node(std::string name) : name_(std::move(name)) {}
    virtual ~Node() = default;

    const std::string& name() const { return name_; }

    virtual void set_armed(bool armed) = 0;
    virtual int device_count() const = 0;
    virtual int online_count() const = 0;
    virtual int armed_count() const = 0;
    virtual Health health() const = 0;
    virtual Node* find(const std::string& name) { return name == name_ ? this : nullptr; }

private:
    std::string name_;
};

// ---- Leaf --------------------------------------------------------------------
class Device final : public Node {
public:
    enum class Kind { Camera, Relay };

    Device(std::string name, Kind kind, bool online)
        : Node(std::move(name)), kind_(kind), online_(online) {}

    void set_armed(bool armed) override { armed_ = armed; }
    int device_count() const override { return 1; }
    int online_count() const override { return online_ ? 1 : 0; }
    int armed_count() const override { return armed_ ? 1 : 0; }
    Health health() const override { return online_ ? Health::Ok : Health::Down; }
    Kind kind() const { return kind_; }

private:
    Kind kind_;
    bool online_;
    bool armed_ = false;
};

// ---- Composite ---------------------------------------------------------------
class Group final : public Node {
public:
    using Node::Node;

    // Child management lives on Group only (the "safe" variant): a Device has
    // no add(), so "add a child to a camera" does not compile.
    Group& add(std::unique_ptr<Node> child) {
        children_.push_back(std::move(child));
        return *this;
    }

    void set_armed(bool armed) override {
        for (auto& c : children_) c->set_armed(armed);
    }
    int device_count() const override { return sum(&Node::device_count); }
    int online_count() const override { return sum(&Node::online_count); }
    int armed_count() const override { return sum(&Node::armed_count); }
    Health health() const override {
        HealthTally t;
        for (const auto& c : children_) t.add(c->health());
        return t.result();
    }
    Node* find(const std::string& name) override {
        if (Node* self = Node::find(name)) return self;
        for (auto& c : children_)
            if (Node* hit = c->find(name)) return hit;
        return nullptr;
    }

private:
    int sum(int (Node::*count)() const) const {
        int n = 0;
        for (const auto& c : children_) n += ((*c).*count)();
        return n;
    }

    std::vector<std::unique_ptr<Node>> children_;
};

// ---- small builders so main() reads like the tree ----------------------------
std::unique_ptr<Node> camera(std::string n, bool online) {
    return std::make_unique<Device>(std::move(n), Device::Kind::Camera, online);
}
std::unique_ptr<Node> relay(std::string n, bool online) {
    return std::make_unique<Device>(std::move(n), Device::Kind::Relay, online);
}
std::unique_ptr<Group> group(std::string n) { return std::make_unique<Group>(std::move(n)); }

int main() {
    auto entrance = group("entrance");
    entrance->add(camera("e1", true)).add(camera("e2", true)).add(relay("door-1", true));
    auto parking = group("parking");
    parking->add(camera("p1", true)).add(camera("p2", false)).add(camera("p3", true));
    auto warehouse = group("warehouse");
    warehouse->add(camera("w1", false)).add(relay("siren-1", true));

    Group site("site-1");
    site.add(std::move(entrance)).add(std::move(parking)).add(std::move(warehouse));

    // Same numbers as naive.cpp.
    CHECK_EQ(site.device_count(), kDevices);
    CHECK_EQ(site.online_count(), kOnline);
    CHECK_EQ(site.find("parking")->health(), kParkingHealth);
    CHECK_EQ(site.health(), kSiteHealth);

    site.find("parking")->set_armed(true);
    CHECK_EQ(site.armed_count(), kArmedAfterParking);
    site.set_armed(true);  // same call, whatever the node is
    CHECK_EQ(site.armed_count(), kArmedAfterSite);
    site.find("entrance")->set_armed(false);
    CHECK_EQ(site.armed_count(), kArmedAfterDisarm);

    // A single device answers the same questions as a whole site.
    Node* p2 = site.find("p2");
    CHECK_EQ(p2->device_count(), 1);
    CHECK_EQ(p2->health(), Health::Down);

    // A deeper layout needs zero new code: building → floors → devices.
    auto floor1 = group("floor-1");
    floor1->add(camera("f1-a", true)).add(camera("f1-b", true));
    auto floor2 = group("floor-2");
    floor2->add(camera("f2-a", false)).add(relay("f2-door", true));
    auto building = group("building-A");
    building->add(std::move(floor1)).add(std::move(floor2));
    Group hq("hq");
    hq.add(std::move(building));

    hq.find("building-A")->set_armed(true);
    CHECK_EQ(hq.armed_count(), 4);
    CHECK_EQ(hq.find("floor-1")->health(), Health::Ok);
    CHECK_EQ(hq.health(), Health::Degraded);

    std::cout << "11-composite/pattern: devices=" << site.device_count()
              << " online=" << site.online_count() << " site=" << site.health()
              << " armed=" << site.armed_count() << "\n";
}
