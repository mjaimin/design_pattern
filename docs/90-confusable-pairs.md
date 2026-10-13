# 90 — Confusable pairs

Several patterns have **the same class diagram** and differ only in **intent**: who decides, what varies, and what the wrapper is for. A reviewer can't tell them apart from the UML. They tell them apart from the questions below.

Each entry has a one-line difference, the code shape of both side by side, and a **review test**: a question you can answer by reading the diff.

---

## Strategy vs State

> **Strategy:** the *client* picks the algorithm. **State:** the *object switches itself* as its internal state changes.

```cpp
// Strategy — 01                                  // State — 10
recorder.set_policy(make_unique<Motion>());       void Connecting::on_connected(Camera& c) {
// chosen from outside; policies                      c.transition_to(make_unique<Streaming>());
// don't know each other                          }   // states know their successors
```

**Review test:** search for who calls the setter. If the setter is only called **from outside** (config, UI, the client), it's Strategy. If the concrete classes call it **on their own context** to move to a sibling, it's State. If States never transition, it's really a Strategy with a misleading name.

## Strategy vs Template Method

> **Strategy:** swap the **whole algorithm** via composition, at runtime. **Template Method:** vary **steps of a fixed skeleton** via inheritance, at compile time.

```cpp
// Strategy — 01                                  // Template Method — 09
class Recorder {                                  class CameraOnboarding {
    std::unique_ptr<RecordingPolicy> policy_;     public:
    void on_frame(const Frame& f) {                   void run() { discover(); auth(); configure(); }  // fixed
        if (policy_->should_record(f)) write(f);  protected:
    }                                                 virtual void auth() = 0;                         // step varies
};                                                };
```

**Review test:** is there a **non-virtual public method that calls protected virtual steps in a fixed order**? Then it's Template Method. Is the varying part **held as a member and replaceable**? Then it's Strategy. One Strategy object often replaces a whole Template Method subclass when the subclasses multiply.

## Adapter vs Facade vs Proxy

> **Adapter:** *changes* an interface to the one callers expect. **Facade:** *simplifies* a whole subsystem behind a new, smaller interface. **Proxy:** *keeps the same* interface and controls access to one object.

```cpp
// Adapter — 03                     // Facade — 04                        // Proxy — 12
class VendorBAdapter                class RecordingService {              class LazyCamera : public Camera {
    : public Camera {               public:                                   std::string snapshot() override {
    std::string rtsp_url() override     void start(CameraId id) {               if (!real_) real_ = connect();
    { return sdk_.GetStreamUri(       session_.open(); demux_.attach();      return real_->snapshot();
        CH_MAIN, PROTO_RTSP); }         muxer_.start(); storage_.alloc();  }
    VendorSdk sdk_;                 }                                         std::unique_ptr<Camera> real_;
};                                  };                                    };
```

| | Wraps | Interface exposed | Purpose |
|---|---|---|---|
| Adapter | one incompatible object | the **client's** existing interface | make it fit |
| Facade | **many** objects (a subsystem) | a **new**, narrower one | make it easy |
| Proxy | one object of the **same** interface | **identical** to the wrapped one | control access (lazy, remote, cache, auth) |

**Review test:** compare the wrapper's interface with the wrapped thing's. **Different, and the client's** → Adapter. **New and smaller, over several objects** → Facade. **Identical** → Proxy (or Decorator, below).

## Decorator vs Proxy

> **Decorator:** *adds behaviour*, and the client composes the layers. **Proxy:** *controls access*, and the proxy usually manages the real object itself.

```cpp
// Decorator — 05                                   // Proxy — 12
auto sink = std::make_unique<RetrySink>(            Camera& cam = registry.get("cam-1");  // a LazyCamera
    std::make_unique<RateLimitSink>(                 // client doesn't know or care that a
        std::make_unique<MqttSink>(cfg)));           // proxy sits in front
// client stacks layers in a chosen order
```

**Review test:** who **builds** the wrapper chain? If **the client stacks it** and the order matters (retry outside or inside rate-limit), it's a Decorator. If the wrapper **creates or looks up the real object itself** (lazy connect, remote stub, cache) and the client receives it pre-made, it's a Proxy. Another giveaway is stacking: decorators are designed to stack, while a proxy-of-a-proxy is rare.

## Bridge vs Strategy

> **Bridge:** *two hierarchies* vary independently, joined for the long term. **Strategy:** *one* behaviour plugged into *one* context, often swapped.

```cpp
// Bridge — 17                                      // Strategy — 01
class Alert {               // abstraction axis      class Recorder {
protected:                                               std::unique_ptr<RecordingPolicy> policy_;
    Transport& transport_;  // implementation axis   };
public:
    virtual void raise() = 0;
};
class IntrusionAlert : public Alert {...};           // one hierarchy (policies), one context
class TamperAlert    : public Alert {...};
// × MqttTransport, WebhookTransport, SmsTransport
```

**Review test:** count hierarchies. If **both sides of the reference have subclasses** (alert kinds × transports) and the alternative would be an N × M class explosion, it's Bridge. If **only the referenced side varies**, it's Strategy. Bridge is designed up front to keep two axes separate. Strategy is usually extracted when one behaviour starts to vary.

## Factory Method vs Abstract Factory

> **Factory Method:** *one* product, chosen by a *subclass* overriding one creation method. **Abstract Factory:** a *family* of related products, chosen by *passing a factory object* in.

```cpp
// Factory Method — 06                              // Abstract Factory — 14
class Recorder {                                     class PlatformFactory {
protected:                                           public:
    virtual std::unique_ptr<SegmentWriter>               virtual std::unique_ptr<Decoder>  decoder()  = 0;
        make_writer() = 0;    // subclass decides        virtual std::unique_ptr<Relay>    relay()    = 0;
};                                                       virtual std::unique_ptr<Watchdog> watchdog() = 0;
class Mp4Recorder : public Recorder {...};           };  // ArmApplianceFactory, X86VmFactory
```

**Review test:** is the creation method **on the class that uses the product**, overridden by its subclasses? Then it's Factory Method, which is inheritance. Is there a **separate object with several `make_*` methods** whose products must match each other? Then it's Abstract Factory, which is composition. An Abstract Factory's methods are often each implemented as factory methods.

## Observer vs Mediator

> **Observer:** one subject *broadcasts* to subscribers it doesn't know, and subscribers don't talk back. **Mediator:** a hub *coordinates* peers that would otherwise call each other, and holds the interaction logic.

```cpp
// Observer — 02                                    // Mediator — 18
detector.subscribe([&](const Event& e) {            void AlarmController::on(const Event& e) {
    recorder.on_event(e);                               if (e.source == keypad && e.kind == Disarm) {
});                                                         siren_.off(); door_.lock(); cams_.normal();
detector.publish(event);  // knows nothing of who       }   // the *rules* of who-reacts-how live here
                          // listens or what they do    }
```

**Review test:** where does the **"when X happens, do Y to Z"** logic live? If it lives **in each subscriber** and the publisher is oblivious, it's Observer. If it lives **in one hub** that knows all the participants and decides who does what, it's Mediator. Mediators are often *implemented* with observer-style notifications, so look at where the decisions are made, not at the subscribe calls.

## Command vs Strategy

> **Command:** *what* to do: a request reified so it can be queued, logged, retried or undone. **Strategy:** *how* to do a job the context has already decided to do.

```cpp
// Command — 08                                     // Strategy — 01
struct OpenRelay final : Command {                  struct MotionTriggered final : RecordingPolicy {
    void execute() override { relay_.open(); }          bool should_record(const Frame&) override;
    void undo() override    { relay_.close(); }     };
};
queue.push(std::make_unique<OpenRelay>(door));      recorder.set_policy(std::make_unique<MotionTriggered>(...));
```

**Review test:** does the object get **stored, queued, sent, logged or undone**, and is it executed **once** by someone other than its creator? Then it's Command. Is it **held by a context and called repeatedly** as part of that context's own work? Then it's Strategy. A Command's interface usually takes no arguments (`execute()`), because everything it needs was captured at creation.

## Composite vs Decorator

> **Composite:** one node holds *many* children and aggregates them, forming a tree. **Decorator:** one wrapper holds *exactly one* inner object and adds to it, forming a chain.

```cpp
// Composite — 11                                   // Decorator — 05
class Zone final : public Device {                  class RetrySink final : public EventSink {
    std::vector<std::unique_ptr<Device>> children_;     std::unique_ptr<EventSink> inner_;   // exactly one
    void arm() override {                               void send(const Event& e) override {
        for (auto& c : children_) c->arm();                 for (int i = 0; i < 3; ++i)
    }                                                           if (inner_->send(e)) return;
};                                                      }
                                                    };
```

**Review test:** count the wrapped references. A **collection** with the operation **fanned out** (and results aggregated) means Composite. A **single** reference that **forwards and adds** something means Decorator. Both share the component interface, which is why the diagrams look alike.

---

## One-glance summary

| Pair | Ask |
|---|---|
| Strategy / State | Who calls the setter? |
| Strategy / Template Method | Composition and runtime, or inheritance with a fixed skeleton? |
| Adapter / Facade / Proxy | Is the wrapper's interface the client's, new, or identical? |
| Decorator / Proxy | Does the client stack it, or does the wrapper manage the real object? |
| Bridge / Strategy | Do one or both sides have subclasses? |
| Factory Method / Abstract Factory | An overridden method, or a passed-in object with a family of `make_*`? |
| Observer / Mediator | Where do the "when X, do Y" rules live? |
| Command / Strategy | Is it stored/queued/undone, or held and called repeatedly? |
| Composite / Decorator | Many children or exactly one? |
