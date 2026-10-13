// 11 Composite — modern form: a recursive std::variant value tree.
//
// No base class, no virtual, no unique_ptr: a Node *is* either a Device or a
// Group, and a Group holds Nodes by value. The whole site can be written as a
// single nested initializer, copied, compared, or serialised like any value.
//
// Operations become free functions. Two kinds show up:
//   * "for every device" operations (arm, counts) — written once as a generic
//     for_each_device() walk plus a lambda;
//   * structural operations (health roll-up, find) — a recursive std::visit.
//
// Trade-off vs pattern.cpp: adding an *operation* is a new free function, no
// class edits (see Visitor, chapter 20). Adding a new *node kind* edits the
// variant and every visit.

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "check.hpp"
#include "fixture.hpp"

struct Device {
    std::string name;
    bool online;
    bool armed = false;
};

struct Node;

struct Group {
    std::string name;
    std::vector<Node> children;  // std::vector of an incomplete type is fine since C++17
};

// Deriving from the variant (rather than a plain alias) lets Group refer to
// Node before Node is complete. std::visit accepts it (C++20 DR P2162).
struct Node : std::variant<Device, Group> {
    using variant::variant;
};

template <class... Fs>
struct overloaded : Fs... {
    using Fs::operator()...;
};

// ---- "for every device" operations -------------------------------------------
// NodeT is Node or const Node, so one walk serves both mutating and read-only
// operations.
template <class NodeT, class F>
void for_each_device(NodeT& n, F&& f) {
    std::visit(
        [&](auto& alt) {
            using T = std::remove_cvref_t<decltype(alt)>;
            if constexpr (std::is_same_v<T, Device>) {
                f(alt);
            } else {
                for (auto& c : alt.children) for_each_device(c, f);
            }
        },
        n);
}

void set_armed(Node& n, bool armed) {
    for_each_device(n, [&](Device& d) { d.armed = armed; });
}
int device_count(const Node& n) {
    int c = 0;
    for_each_device(n, [&](const Device&) { ++c; });
    return c;
}
int online_count(const Node& n) {
    int c = 0;
    for_each_device(n, [&](const Device& d) { c += d.online; });
    return c;
}
int armed_count(const Node& n) {
    int c = 0;
    for_each_device(n, [&](const Device& d) { c += d.armed; });
    return c;
}

// ---- structural operations ---------------------------------------------------
Health health(const Node& n) {
    return std::visit(overloaded{
                          [](const Device& d) { return d.online ? Health::Ok : Health::Down; },
                          [](const Group& g) {
                              HealthTally t;
                              for (const auto& c : g.children) t.add(health(c));
                              return t.result();
                          },
                      },
                      n);
}

Node* find(Node& n, const std::string& name) {
    return std::visit(overloaded{
                          [&](Device& d) -> Node* { return d.name == name ? &n : nullptr; },
                          [&](Group& g) -> Node* {
                              if (g.name == name) return &n;
                              for (auto& c : g.children)
                                  if (Node* hit = find(c, name)) return hit;
                              return nullptr;
                          },
                      },
                      n);
}

int main() {
    // The whole tree as one value.
    Node site = Group{"site-1",
                      {
                          Group{"entrance", {Device{"e1", true}, Device{"e2", true}, Device{"door-1", true}}},
                          Group{"parking", {Device{"p1", true}, Device{"p2", false}, Device{"p3", true}}},
                          Group{"warehouse", {Device{"w1", false}, Device{"siren-1", true}}},
                      }};

    CHECK_EQ(device_count(site), kDevices);
    CHECK_EQ(online_count(site), kOnline);
    CHECK_EQ(health(*find(site, "parking")), kParkingHealth);
    CHECK_EQ(health(site), kSiteHealth);

    set_armed(*find(site, "parking"), true);
    CHECK_EQ(armed_count(site), kArmedAfterParking);
    set_armed(site, true);
    CHECK_EQ(armed_count(site), kArmedAfterSite);
    set_armed(*find(site, "entrance"), false);
    CHECK_EQ(armed_count(site), kArmedAfterDisarm);

    // Value semantics: a copy is a deep, independent tree.
    Node snapshot = site;
    set_armed(site, false);
    CHECK_EQ(armed_count(site), 0);
    CHECK_EQ(armed_count(snapshot), kArmedAfterDisarm);

    std::cout << "11-composite/modern: variant tree matches pattern.cpp\n";
}
