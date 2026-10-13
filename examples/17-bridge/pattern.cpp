// 17 Bridge — classic form.
//
// Force: two things vary independently — *what happened* (intrusion, tamper,
//        camera offline; product adds kinds) and *how it is delivered* (MQTT,
//        webhook, SMS; integrations add transports). Neither axis may force
//        edits on the other, and growth must be N + M classes, not N × M.
//
// Roles
//   Abstraction         : Alert — has-a Transport; send() is written once
//   Refined abstractions: IntrusionAlert, TamperAlert, CameraOfflineAlert
//   Implementor         : Transport — the delivery primitive
//   Concrete implementors: MqttTransport, WebhookTransport, SmsTransport
//
// The "bridge" is the Transport& member: the abstraction hierarchy talks to
// the implementor hierarchy only through it.

#include <string>
#include <vector>

#include "check.hpp"
#include "expected.hpp"

enum class Severity { Low, High };

inline std::string lower(Severity s) { return s == Severity::High ? "high" : "low"; }
inline std::string upper(Severity s) { return s == Severity::High ? "HIGH" : "LOW"; }

// ---- Implementor ------------------------------------------------------------
class Transport {
public:
    virtual ~Transport() = default;
    virtual void deliver(Severity sev, const std::string& title, const std::string& body) = 0;
    const std::vector<std::string>& sent() const { return sent_; }

protected:
    void record(std::string wire) { sent_.push_back(std::move(wire)); }  // stands in for the network

private:
    std::vector<std::string> sent_;
};

// ---- Concrete implementors: each wire format written exactly once ----------
class MqttTransport final : public Transport {
public:
    void deliver(Severity sev, const std::string& title, const std::string& body) override {
        record("alerts/" + lower(sev) + " " + title + ": " + body);
    }
};

class WebhookTransport final : public Transport {
public:
    void deliver(Severity sev, const std::string& title, const std::string& body) override {
        record(R"({"severity":")" + lower(sev) + R"(","title":")" + title + R"(","body":")" + body + R"("})");
    }
};

class SmsTransport final : public Transport {
public:
    void deliver(Severity sev, const std::string& title, const std::string& body) override {
        record((upper(sev) + " " + title + ": " + body).substr(0, kSmsMax));
    }
};

// ---- Abstraction ------------------------------------------------------------
class Alert {
public:
    explicit Alert(Transport& t) : transport_(t) {}
    virtual ~Alert() = default;

    void send() { transport_.deliver(severity(), title(), body()); }  // written once

protected:
    virtual Severity severity() const = 0;
    virtual std::string title() const = 0;
    virtual std::string body() const = 0;

private:
    Transport& transport_;  // the bridge
};

// ---- Refined abstractions: each alert kind written exactly once -------------
class IntrusionAlert final : public Alert {
public:
    IntrusionAlert(Transport& t, std::string zone) : Alert(t), zone_(std::move(zone)) {}

protected:
    Severity severity() const override { return Severity::High; }
    std::string title() const override { return "Intrusion"; }
    std::string body() const override { return "zone=" + zone_; }

private:
    std::string zone_;
};

class TamperAlert final : public Alert {
public:
    TamperAlert(Transport& t, std::string cam) : Alert(t), cam_(std::move(cam)) {}

protected:
    Severity severity() const override { return Severity::High; }
    std::string title() const override { return "Tamper"; }
    std::string body() const override { return "camera=" + cam_ + " view blocked"; }

private:
    std::string cam_;
};

class CameraOfflineAlert final : public Alert {
public:
    CameraOfflineAlert(Transport& t, std::string cam, int minutes)
        : Alert(t), cam_(std::move(cam)), minutes_(minutes) {}

protected:
    Severity severity() const override { return Severity::Low; }
    std::string title() const override { return "Camera offline"; }
    std::string body() const override {
        return "camera=" + cam_ + " for " + std::to_string(minutes_) + " min";
    }

private:
    std::string cam_;
    int minutes_;
};

int main() {
    MqttTransport mqtt;
    WebhookTransport webhook;
    SmsTransport sms;

    // Any kind × any transport, combined at runtime: 3 + 3 classes, 9 pairs.
    for (Transport* t : {static_cast<Transport*>(&mqtt), static_cast<Transport*>(&webhook),
                         static_cast<Transport*>(&sms)}) {
        IntrusionAlert(*t, "entrance").send();
        TamperAlert(*t, "cam-2").send();
        CameraOfflineAlert(*t, "cam-3", 15).send();
    }

    // Same bytes as naive.cpp.
    CHECK_EQ(mqtt.sent().size(), kPerTransport);
    CHECK_EQ(webhook.sent().size(), kPerTransport);
    CHECK_EQ(sms.sent().size(), kPerTransport);
    CHECK_EQ(mqtt.sent()[0], kMqttIntrusion);
    CHECK_EQ(webhook.sent()[1], kWebhookTamper);
    CHECK_EQ(sms.sent()[2], kSmsOffline);

    std::cout << "17-bridge/pattern: 3 kinds + 3 transports, 9 messages (" << kPerTransport << " per transport)\n";
}
