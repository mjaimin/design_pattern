// 20 Visitor — the naive version.
//
// A site is a tree: gateways own cameras, relays and sensors. The device
// classes are stable — they change maybe once a year. The *operations* over
// the tree keep arriving: licence count, health report, CSV export, then
// firmware audit, then power budget...
//
// Here every operation is a free function that walks the tree and asks each
// node "what are you?" with dynamic_cast.
//
// Where it hurts:
//   - every operation repeats the same traversal and the same type tests;
//   - add an `Nvr` device type and *nothing tells you* which operations need
//     a new branch — the final `return 0` / fall-through silently swallows it;
//   - the alternative (a virtual per operation on Device) means editing every
//     device class for every new report — Divergent Change on stable classes.
// Smell: type tests (Switch Statements in disguise) scattered across functions.

#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "expected.hpp"

class Device {
public:
    Device(std::string id, bool online) : id_(std::move(id)), online_(online) {}
    virtual ~Device() = default;  // needed only so dynamic_cast works
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

private:
    bool ai_;
};

class Relay final : public Device {
public:
    using Device::Device;
};
class Sensor final : public Device {
public:
    using Device::Device;
};

class Gateway final : public Device {
public:
    using Device::Device;
    void add(std::unique_ptr<Device> d) { children_.push_back(std::move(d)); }
    const std::vector<std::unique_ptr<Device>>& children() const { return children_; }

private:
    std::vector<std::unique_ptr<Device>> children_;
};

// ---- Operation 1 -----------------------------------------------------------
int licences(const Device& d) {
    if (auto* g = dynamic_cast<const Gateway*>(&d)) {
        int n = 0;
        for (const auto& c : g->children()) n += licences(*c);
        return n;
    }
    if (auto* cam = dynamic_cast<const Camera*>(&d)) return cam->ai_enabled() ? 2 : 1;
    return 0;  // relays, sensors — and, silently, any type added later
}

// ---- Operation 2 -----------------------------------------------------------
void offline(const Device& d, std::vector<std::string>& out) {
    if (auto* g = dynamic_cast<const Gateway*>(&d)) {
        if (!g->online()) out.push_back(g->id());
        for (const auto& c : g->children()) offline(*c, out);
        return;
    }
    if (!d.online()) out.push_back(d.id());
}

// ---- Operation 3 -----------------------------------------------------------
void csv(const Device& d, std::string& out) {
    const char* status = d.online() ? "online" : "offline";
    if (auto* g = dynamic_cast<const Gateway*>(&d)) {
        out += std::string("gateway,") + g->id() + "," + status + "\n";
        for (const auto& c : g->children()) csv(*c, out);
    } else if (dynamic_cast<const Camera*>(&d)) {
        out += "camera," + d.id() + "," + status + "\n";
    } else if (dynamic_cast<const Relay*>(&d)) {
        out += "relay," + d.id() + "," + status + "\n";
    } else if (dynamic_cast<const Sensor*>(&d)) {
        out += "sensor," + d.id() + "," + status + "\n";
    }  // a new type: no row, no error
}

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

    int lic = 0;
    std::vector<std::string> down;
    std::string rows;
    for (const auto& g : site) {
        lic += licences(*g);
        offline(*g, down);
        csv(*g, rows);
    }

    CHECK_EQ(lic, kLicences);
    CHECK(down == kOffline);
    CHECK_EQ(rows, kCsv);

    std::cout << "20-visitor/naive: licences=" << lic << " offline=" << down.size()
              << " csv_rows=8\n";
}
