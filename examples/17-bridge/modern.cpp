// 17 Bridge — both axes without class hierarchies.
//
// Axis 1 (alert kind) is a closed, data-only set → std::variant of plain
//   structs, turned into a neutral Message by one std::visit.
// Axis 2 (transport) is "one operation: put this message on the wire"
//   → std::function<void(const Message&)>.
// The bridge is the Message struct: kinds produce it, transports consume it,
// and neither side names the other. Still N + M pieces of code.
//
// Pick this when kinds carry data but no behaviour beyond "describe me", and
// transports have exactly one operation. Pick the class form (pattern.cpp)
// when either side grows several operations or must be open to plugins.

#include <functional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "check.hpp"
#include "expected.hpp"

// ---- the bridge: a neutral message ------------------------------------------
struct Message {
    bool high;
    std::string title;
    std::string body;
};

// ---- axis 1: kinds as data ----------------------------------------------------
struct Intrusion { std::string zone; };
struct Tamper { std::string camera; };
struct CameraOffline { std::string camera; int minutes; };

using Alert = std::variant<Intrusion, Tamper, CameraOffline>;

Message to_message(const Alert& a) {
    return std::visit(
        [](const auto& k) -> Message {
            using K = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<K, Intrusion>)
                return {true, "Intrusion", "zone=" + k.zone};
            else if constexpr (std::is_same_v<K, Tamper>)
                return {true, "Tamper", "camera=" + k.camera + " view blocked"};
            else
                return {false, "Camera offline",
                        "camera=" + k.camera + " for " + std::to_string(k.minutes) + " min"};
        },
        a);
}

// ---- axis 2: transports as functions ------------------------------------------
using Transport = std::function<void(const Message&)>;

Transport mqtt(std::vector<std::string>& wire) {
    return [&wire](const Message& m) {
        wire.push_back(std::string("alerts/") + (m.high ? "high" : "low") + " " + m.title + ": " + m.body);
    };
}

Transport webhook(std::vector<std::string>& wire) {
    return [&wire](const Message& m) {
        wire.push_back(std::string(R"({"severity":")") + (m.high ? "high" : "low") + R"(","title":")" +
                       m.title + R"(","body":")" + m.body + R"("})");
    };
}

Transport sms(std::vector<std::string>& wire) {
    return [&wire](const Message& m) {
        wire.push_back((std::string(m.high ? "HIGH" : "LOW") + " " + m.title + ": " + m.body).substr(0, kSmsMax));
    };
}

void send(const Alert& a, const Transport& t) { t(to_message(a)); }

int main() {
    std::vector<std::string> mqtt_wire, webhook_wire, sms_wire;
    const std::vector<Transport> transports = {mqtt(mqtt_wire), webhook(webhook_wire), sms(sms_wire)};
    const std::vector<Alert> alerts = {Intrusion{"entrance"}, Tamper{"cam-2"}, CameraOffline{"cam-3", 15}};

    for (const auto& t : transports)
        for (const auto& a : alerts) send(a, t);

    CHECK_EQ(mqtt_wire.size(), kPerTransport);
    CHECK_EQ(webhook_wire.size(), kPerTransport);
    CHECK_EQ(sms_wire.size(), kPerTransport);
    CHECK_EQ(mqtt_wire[0], kMqttIntrusion);
    CHECK_EQ(webhook_wire[1], kWebhookTamper);
    CHECK_EQ(sms_wire[2], kSmsOffline);

    std::cout << "17-bridge/modern: variant kinds × function transports, same 9 messages\n";
}
