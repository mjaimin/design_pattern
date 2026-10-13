// 17 Bridge — the naive version: one class per (alert kind × transport).
//
// Alerts were first sent only over MQTT. Then customers wanted webhooks, then
// SMS. Each time, every alert kind got another subclass.
//
//                     Alert
//          ┌────────────┼──────────────┐
//   IntrusionAlert   TamperAlert   OfflineAlert         ← axis 1: what happened
//     ├ Mqtt…          ├ Mqtt…       ├ Mqtt…
//     ├ Webhook…       ├ Webhook…    ├ Webhook…         ← axis 2: how it is delivered
//     └ Sms…           └ Sms…        └ Sms…
//
// Where it hurts:
//   - 3 kinds × 3 transports = 9 leaf classes. A 4th transport adds 3 more;
//     a 4th kind adds 3 more. Growth is N × M.
//   - The MQTT format is written three times (once per kind). Fix a bug in it
//     and you must find all three copies.
//   - The two axes change for different reasons (product decides kinds,
//     integrations team decides transports) but live in one hierarchy.

#include <string>
#include <vector>

#include "check.hpp"
#include "expected.hpp"

struct Outboxes {
    std::vector<std::string> mqtt, webhook, sms;
};

class Alert {
public:
    explicit Alert(Outboxes& out) : out_(out) {}
    virtual ~Alert() = default;
    virtual void send() = 0;

protected:
    Outboxes& out_;
};

// ---- axis 1: kinds ----------------------------------------------------------
class IntrusionAlert : public Alert {
public:
    IntrusionAlert(Outboxes& out, std::string zone) : Alert(out), zone_(std::move(zone)) {}

protected:
    std::string zone_;
};

class TamperAlert : public Alert {
public:
    TamperAlert(Outboxes& out, std::string cam) : Alert(out), cam_(std::move(cam)) {}

protected:
    std::string cam_;
};

class OfflineAlert : public Alert {
public:
    OfflineAlert(Outboxes& out, std::string cam, int minutes)
        : Alert(out), cam_(std::move(cam)), minutes_(minutes) {}

protected:
    std::string cam_;
    int minutes_;
};

// ---- axis 2, multiplied by axis 1: nine leaves -------------------------------
struct MqttIntrusionAlert final : IntrusionAlert {
    using IntrusionAlert::IntrusionAlert;
    void send() override { out_.mqtt.push_back("alerts/high Intrusion: zone=" + zone_); }
};
struct WebhookIntrusionAlert final : IntrusionAlert {
    using IntrusionAlert::IntrusionAlert;
    void send() override {
        out_.webhook.push_back(R"({"severity":"high","title":"Intrusion","body":"zone=)" + zone_ + R"("})");
    }
};
struct SmsIntrusionAlert final : IntrusionAlert {
    using IntrusionAlert::IntrusionAlert;
    void send() override { out_.sms.push_back(("HIGH Intrusion: zone=" + zone_).substr(0, kSmsMax)); }
};

struct MqttTamperAlert final : TamperAlert {
    using TamperAlert::TamperAlert;
    void send() override { out_.mqtt.push_back("alerts/high Tamper: camera=" + cam_ + " view blocked"); }
};
struct WebhookTamperAlert final : TamperAlert {
    using TamperAlert::TamperAlert;
    void send() override {
        out_.webhook.push_back(R"({"severity":"high","title":"Tamper","body":"camera=)" + cam_ +
                               R"( view blocked"})");
    }
};
struct SmsTamperAlert final : TamperAlert {
    using TamperAlert::TamperAlert;
    void send() override {
        out_.sms.push_back(("HIGH Tamper: camera=" + cam_ + " view blocked").substr(0, kSmsMax));
    }
};

struct MqttOfflineAlert final : OfflineAlert {
    using OfflineAlert::OfflineAlert;
    void send() override {
        out_.mqtt.push_back("alerts/low Camera offline: camera=" + cam_ + " for " +
                            std::to_string(minutes_) + " min");
    }
};
struct WebhookOfflineAlert final : OfflineAlert {
    using OfflineAlert::OfflineAlert;
    void send() override {
        out_.webhook.push_back(R"({"severity":"low","title":"Camera offline","body":"camera=)" + cam_ +
                               " for " + std::to_string(minutes_) + R"( min"})");
    }
};
struct SmsOfflineAlert final : OfflineAlert {
    using OfflineAlert::OfflineAlert;
    void send() override {
        out_.sms.push_back(
            ("LOW Camera offline: camera=" + cam_ + " for " + std::to_string(minutes_) + " min")
                .substr(0, kSmsMax));
    }
};

int main() {
    Outboxes out;
    MqttIntrusionAlert(out, "entrance").send();
    WebhookIntrusionAlert(out, "entrance").send();
    SmsIntrusionAlert(out, "entrance").send();
    MqttTamperAlert(out, "cam-2").send();
    WebhookTamperAlert(out, "cam-2").send();
    SmsTamperAlert(out, "cam-2").send();
    MqttOfflineAlert(out, "cam-3", 15).send();
    WebhookOfflineAlert(out, "cam-3", 15).send();
    SmsOfflineAlert(out, "cam-3", 15).send();

    CHECK_EQ(out.mqtt.size(), kPerTransport);
    CHECK_EQ(out.webhook.size(), kPerTransport);
    CHECK_EQ(out.sms.size(), kPerTransport);
    CHECK_EQ(out.mqtt[0], kMqttIntrusion);
    CHECK_EQ(out.webhook[1], kWebhookTamper);
    CHECK_EQ(out.sms[2], kSmsOffline);

    std::cout << "17-bridge/naive: 9 leaf classes, 9 messages (" << kPerTransport << " per transport)\n";
}
