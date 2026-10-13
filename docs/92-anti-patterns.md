# 92 — Anti-patterns

A pattern applied without a force is just indirection: more files and more hops between symptom and cause, with nothing gained. These are the common ways patterns go wrong, each with a short C++ example and the fix.

---

## 1. Singleton as a disguised global

```cpp
class Config {
public:
    static Config& instance() { static Config c; return c; }
    int retention_days() const;
};

void Recorder::purge_old() {
    auto days = Config::instance().retention_days();   // hidden dependency
}
```

What goes wrong:
- **Hidden dependencies.** `Recorder`'s constructor says nothing about needing `Config`. You find out by reading every method.
- **Untestable.** A test can't give `Recorder` a 1-day retention without mutating global state, which then leaks into the next test.
- **Order and lifetime.** Singletons constructed from other singletons' constructors, or used during static destruction, cause bugs that only show up at shutdown.
- **Concurrency.** Function-local statics are initialised thread-safely, but everything *after* initialisation is your problem, and the singleton is now shared by every thread.

**Fix:** pass the dependency in. Construct one instance in `main()` (the *composition root*) and hand it to whoever needs it.

```cpp
class Recorder {
public:
    explicit Recorder(const RetentionPolicy& retention) : retention_(retention) {}
private:
    const RetentionPolicy& retention_;
};
```

"Exactly one" is a fact about the **application's wiring**, not about the **class**. See [15 — Singleton](15-singleton.md).

## 2. Factory of factories

```cpp
auto factory = CameraFactoryProvider::instance()
                   .get_factory_registry()
                   .lookup<CameraDriverFactoryFactory>("onvif")
                   ->create_factory(cfg)
                   ->create(device);
```

Each layer was added "for flexibility", but no layer has a second implementation. Reading this tells you nothing about which class you end up with.

**Fix:** one creation point, at the level where the choice is really made.

```cpp
std::unique_ptr<Camera> make_camera(const DeviceInfo& d) {
    if (d.protocol == "onvif") return std::make_unique<OnvifCamera>(d);
    if (d.vendor == "vendor-b") return std::make_unique<VendorBCamera>(d);
    throw std::invalid_argument("unsupported device: " + d.vendor);
}
```

A plain function with an `if` chain *is* a factory. Promote it to a registry only when drivers are loaded at runtime (plugins) and the `if` chain can't be edited.

## 3. Speculative generality

```cpp
template <class Clock, class Storage, class Codec, class Allocator, class Logger>
class Recorder { ... };   // one instantiation exists in the whole codebase
```

Every parameter, hook and interface was added for a future that hasn't arrived. Each one costs compile time, error-message length and reading effort, and each was designed from **one** case. When the second case does arrive, it rarely fits the guessed abstraction.

**Fix:** delete the unused generality. Keep the **seam** (a clear function boundary where the abstraction *would* go) and write down what would trigger it: *"When a second codec has a ticket, `encode()` becomes a Strategy."* Extracting an interface from two real cases costs less than redesigning one guessed from a single case.

## 4. Pattern-name theatre

```cpp
class RecordingStrategyFactoryManagerImpl : public AbstractRecordingStrategyFactoryManager { ... };
```

Pattern names in class names describe the **mechanism**, not the **domain**. The reader learns how the code is built, but not what it's for. Usually the structure doesn't even match the name: the "Strategy" is never swapped, and the "Factory" makes one type.

**Fix:** name classes after domain concepts (`RecordingPolicy`, `MotionTriggeredPolicy`, `CameraDriverRegistry`). If a pattern is worth mentioning, put it in **one comment** that also states the force: `// Strategy: operators switch modes at runtime.`

## 5. Interface with one implementation

```cpp
class IClipUploader { public: virtual void upload(const Clip&) = 0; virtual ~IClipUploader() = default; };
class ClipUploader final : public IClipUploader { ... };   // the only one, forever
```

Every call is now a virtual call. "Go to definition" lands on the interface. Every change edits two files, and the interface encodes the one implementation's quirks anyway.

**Fix:** use the concrete class. Extract the interface **when a second implementation exists**. A test double counts *if you actually need one*, meaning the real thing does I/O you can't run in a test. For the test-double case, consider whether a seam at a lower level (inject an HTTP client, not an uploader) gives you one interface instead of many. See [00 — Dependency Inversion](00-foundations.md#dependency-inversion-concretely).

## 6. Deep inheritance for code reuse

```cpp
class Device { /* logging, retry, config */ };
class NetworkDevice : public Device { /* HTTP helpers */ };
class Camera : public NetworkDevice { /* RTSP */ };
class PtzCamera : public Camera { /* PTZ */ };
class ThermalPtzCamera : public PtzCamera { /* thermal */ };
class ThermalPtzCameraWithAudio : public ThermalPtzCamera { ... };  // and so on
```

The hierarchy was built to **share code**, not to express substitutability. Problems:
- **Fragile base class.** A change in `Device` ripples through five levels.
- **Combinatorial explosion.** Is a thermal camera with audio but no PTZ a new branch?
- **Liskov violations.** `PtzCamera::move()` on a model whose PTZ is disabled throws, so the subtype isn't substitutable.

**Fix:** compose capabilities. Keep inheritance for genuine *is-a* interfaces, and only one level deep.

```cpp
class Camera {
public:
    Ptz*   ptz();     // nullptr if not supported
    Audio* audio();
private:
    std::unique_ptr<Ptz> ptz_;
    std::unique_ptr<Audio> audio_;
};
```

Retry, logging and HTTP helpers become **members or free functions**, not base classes.

## 7. God Mediator

```cpp
class SiteController {   // 3 000 lines
    void on_event(const Event& e) {
        if (e.kind == Motion && armed_ && zone_of(e) == "perimeter" && !maintenance_ && ...) { ... }
        if (e.kind == DoorForced) { ... siren_ ... cams_ ... relays_ ... notify_ ... }
        // 140 more branches
    }
};
```

A Mediator was introduced to stop peers calling each other, which is good. Then every rule went into it. It's now the only class anyone edits, it knows every peer's internals, and no part of it can be tested alone. The many-to-many coupling has been **concentrated** in one place, not removed.

**Fix:** keep the mediator as **routing**, and move each rule into its own object. That's Command or Chain of Responsibility for the rules, or a rule table interpreted by an Interpreter (chapter 23). Split by concern, so alarm logic, recording logic and access control each get their own coordinator. A mediator should read like a wiring diagram, not like a rule book.

## 8. Observer leaks and dangling callbacks

```cpp
class LiveView {
public:
    explicit LiveView(EventBus& bus) {
        bus.subscribe([this](const Event& e) { render(e); });   // captures raw `this`
    }
};   // LiveView destroyed → bus still holds the lambda → next publish calls into freed memory
```

C++ doesn't garbage-collect subscribers. Forgetting to unsubscribe has two consequences:
- **Dangling `this`:** a use-after-free on the next publish. It's often intermittent, because it depends on whether the memory was reused.
- **Leaks:** if the lambda captures a `shared_ptr` to its owner, the bus keeps the owner alive forever.
- **Reentrancy:** a subscriber that unsubscribes (or subscribes) from inside a callback invalidates the bus's iteration.

**Fix:** tie the subscription's lifetime to the subscriber with an **RAII handle**.

```cpp
class Subscription {           // move-only; destructor unsubscribes
public:
    ~Subscription() { if (bus_) bus_->unsubscribe(id_); }
    // ...
};

class LiveView {
    Subscription sub_;         // declared last → destroyed first → unsubscribes before members die
public:
    explicit LiveView(EventBus& bus) : sub_(bus.subscribe([this](const Event& e) { render(e); })) {}
};
```

Alternatives: `std::weak_ptr` listeners that are skipped and pruned once expired; or Boost.Signals2's `scoped_connection`. For reentrancy, iterate over a **copy** of the subscriber list, or defer removals until the publish finishes. See [02 — Observer](02-observer.md).

---

## The common root

Every entry above is the same mistake: **structure added without a named force**.

| Check before adding indirection | If the answer is no |
|---|---|
| Do two variants exist **today**? | Write the direct code and mark the seam. |
| Can you **name** who or what makes it vary? | It's speculation. Delete it. |
| Is the direct version **genuinely worse**? Sketch it. | The direct version wins. |
| Does the abstraction make the code **easier to test**? | It's probably in the wrong place. |

Refactoring *into* a pattern later, when the second case is real, is cheap. Removing a wrong abstraction after twenty callers have adapted to it is not.
