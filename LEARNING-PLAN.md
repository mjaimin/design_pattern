# Learning Plan — GoF Design Patterns in Modern C++

Goal: for any piece of code, be able to say **what is going to change, what must stay still**, which pattern (if any) answers that — and **when the direct code is better**.

Language: **C++ only — C++20** (g++ 13, CMake). Every example is drawn from one domain: a **video-surveillance edge gateway** — cameras, streams, recording, detection events, relays/alarms, sites. Generic by design: no vendor or company internals.

Method per pattern: **read the force → run `naive` → run `pattern` → do the exercise → write a note in my own words → answer the checkpoint**.

---

## How each pattern is explained (fixed template)

Every `docs/NN-<pattern>.md` follows the same nine sections, so patterns can be compared side by side:

| # | Section | What it answers |
|---|---|---|
| 1 | **Force** | One sentence: *what varies, who makes it vary, what must stay still* |
| 2 | **Naive code & where it hurts** | The direct version (`naive.cpp`) and the exact change that makes it painful |
| 3 | **Structure** | Roles + Mermaid class/sequence diagram |
| 4 | **Gateway example** | Walkthrough of `pattern.cpp` |
| 5 | **Modern C++ form** | Classic virtual-based version vs `std::function` / templates / CRTP / `std::variant` / RAII — and when each wins |
| 6 | **When NOT to use** | The YAGNI gate: one implementation + a hypothetical is not a reason |
| 7 | **Confused with** | The neighbour pattern and the one-line difference |
| 8 | **Seen in the wild** | STL, Boost, Qt, GStreamer, Linux kernel, etc. |
| 9 | **Exercise + checkpoint** | A change to make in the code, and a question to answer from memory |

Each `examples/NN-<pattern>/` holds `naive.cpp`, `pattern.cpp` (and `modern.cpp` where the idiomatic C++ form differs a lot). Each binary self-checks with assertions and is registered with `ctest`.

---

## Order — by how often you'll meet it, not by book chapter

### Part 0 — Foundations

| # | Module | Key exercise |
|---|---|---|
| 00 | Program to an interface, composition over inheritance, SOLID (OCP, DIP), the 4 kinds of polymorphism in C++ (virtual, templates, `std::function`/type erasure, `std::variant`) | Implement one "camera" four ways and compare |

### Part A — Core: the 10 you will use constantly

| # | Pattern | Family | Gateway example | Force |
|---|---|---|---|---|
| 01 | **Strategy** | Behavioral | Recording policy: continuous / motion-triggered / scheduled | Policy varies per camera at runtime; recorder loop stays fixed |
| 02 | **Observer** | Behavioral | Detection event fans out to recorder, alarm relay, cloud uploader, live UI | Number of reactors grows; detector must not know them |
| 03 | **Adapter** | Structural | Vendor A HTTP API, Vendor B SDK, ONVIF → one `Camera` interface | Third-party interfaces vary; gateway code must not |
| 04 | **Facade** | Structural | `recorder.start(cameraId)` hides RTSP session, depayloader, muxer, storage allocator | Subsystem is messy; callers want one door |
| 05 | **Decorator** | Structural | Event sink + logging / retry / rate-limit / metrics, stacked at runtime | Optional behaviours combine freely |
| 06 | **Factory Method** | Creational | `Recorder` subclasses decide which segment writer (MP4 / MKV / raw) to make | Product type varies with the creator subclass |
| 07 | **Builder** | Creational | Media pipeline: source → decode → infer → encode → sink, step by step with validation | Many optional parts; invalid combos must be impossible |
| 08 | **Command** | Behavioral | Relay / PTZ actions as objects: queued, logged, retried, undone | Actions must be first-class data |
| 09 | **Template Method** | Behavioral | Camera onboarding: discover → auth → fetch profiles → configure, vendor overrides steps | Skeleton fixed; steps vary |
| 10 | **State** | Behavioral | Camera link lifecycle: Disconnected → Connecting → Streaming → Reconnecting (backoff) → Offline | Behaviour changes wholesale with state |

### Part B — Common: you'll meet these in real codebases

| # | Pattern | Family | Gateway example | Force |
|---|---|---|---|---|
| 11 | **Composite** | Structural | Site → zones → cameras/relays: arm a zone arms all children; health rolls up | Treat one device and a tree of devices the same |
| 12 | **Proxy** | Structural | Lazy-connect camera, cached snapshot, permission check before PTZ | Control or defer access to the real object |
| 13 | **Chain of Responsibility** | Behavioral | Event filters: zone mask → schedule → confidence → cooldown/dedupe → dispatch | A request passes ordered handlers, any can stop it |
| 14 | **Abstract Factory** | Creational | Platform family: ARM appliance (HW decoder, GPIO relay, HW watchdog) vs x86 VM (SW decoder, simulated relay, systemd watchdog) | Whole families must stay consistent |
| 15 | **Singleton** | Creational | Logger / config — then rewritten with dependency injection | Exactly one instance — and why that's usually a disguised global |
| 16 | **Iterator** | Behavioral | Walk recorded segments across date-partitioned storage; as a C++20 range | Traverse without exposing layout |

### Part C — Specialised: know them, use them rarely

| # | Pattern | Family | Gateway example | Force |
|---|---|---|---|---|
| 17 | **Bridge** | Structural | Alert kind (intrusion / tamper / offline) × transport (MQTT / webhook / SMS) | Two axes vary independently — avoid N×M classes |
| 18 | **Mediator** | Behavioral | Alarm controller coordinating door relay, siren, cameras, keypad | Many-to-many chatter needs a hub |
| 19 | **Memento** | Behavioral | Snapshot camera config before a push; roll back if the camera rejects it | Undo without breaking encapsulation |
| 20 | **Visitor** | Behavioral | Over the device tree: health report, JSON export, licence count — then `std::visit` | New operations over a stable structure |
| 21 | **Prototype** | Creational | Clone a configured camera template to onboard 50 identical cameras | Cloning beats constructing |
| 22 | **Flyweight** | Structural | Thousands of bounding boxes share class metadata (label, colour, icon) | Many near-identical objects eat memory |
| 23 | **Interpreter** | Behavioral | Rule DSL: `person AND zone=entrance AND time 22:00-06:00` → alert | A small language needs evaluating |

### Comparison modules (after each Part)

| # | Module | Content |
|---|---|---|
| 90 | **Confusable pairs** | Strategy vs State · Strategy vs Template Method · Adapter vs Facade vs Proxy · Decorator vs Proxy · Bridge vs Strategy · Factory Method vs Abstract Factory · Observer vs Mediator · Command vs Strategy |
| 91 | **Force → pattern cheat sheet** | One table: "what varies" → pattern → C++ idiom |
| 92 | **Anti-patterns** | Singleton abuse, factories of factories, speculative generality, pattern-name theatre |

### Capstone — `mini-gateway`

A small simulated gateway: fake cameras emit frames → detector emits events → rule chain filters → observers record clips and fire relay commands, all behind a facade. Document **every pattern used and why**, and at least **three places where I chose *not* to use a pattern**.

---

## Repo layout

```
design_pattern/
├── README.md               index: all 23 by family + by learning order
├── LEARNING-PLAN.md        this file
├── CMakeLists.txt          builds every example; `ctest` runs all
├── common/                 tiny shared domain types (Camera, Frame, DetectionEvent, ...)
├── docs/
│   ├── 00-foundations.md
│   ├── 01-strategy.md … 23-interpreter.md
│   └── 90-confusable-pairs.md, 91-cheat-sheet.md, 92-anti-patterns.md
├── examples/
│   └── NN-<pattern>/ naive.cpp  pattern.cpp  [modern.cpp]
├── notes/                  my own words, one per pattern
└── capstone/mini-gateway/
```

Build & run:

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
./build/examples/01-strategy/pattern
```

## Note template (each `notes/NN-<pattern>.md`)

```markdown
# <Pattern>
## Force in one sentence
## Where I've seen this (work / libraries)
## The C++ form I'd actually write
## When I would NOT use it
## Checkpoint answer
## Open questions
```

---

## Progress

| # | Pattern | Read | Ran naive/pattern | Exercise | Note | Checkpoint |
|---|---|---|---|---|---|---|
| 00 | Foundations | ☐ | ☐ | ☐ | ☐ | ☐ |
| 01 | Strategy | ☐ | ☐ | ☐ | ☐ | ☐ |
| 02 | Observer | ☐ | ☐ | ☐ | ☐ | ☐ |
| 03 | Adapter | ☐ | ☐ | ☐ | ☐ | ☐ |
| 04 | Facade | ☐ | ☐ | ☐ | ☐ | ☐ |
| 05 | Decorator | ☐ | ☐ | ☐ | ☐ | ☐ |
| 06 | Factory Method | ☐ | ☐ | ☐ | ☐ | ☐ |
| 07 | Builder | ☐ | ☐ | ☐ | ☐ | ☐ |
| 08 | Command | ☐ | ☐ | ☐ | ☐ | ☐ |
| 09 | Template Method | ☐ | ☐ | ☐ | ☐ | ☐ |
| 10 | State | ☐ | ☐ | ☐ | ☐ | ☐ |
| 11 | Composite | ☐ | ☐ | ☐ | ☐ | ☐ |
| 12 | Proxy | ☐ | ☐ | ☐ | ☐ | ☐ |
| 13 | Chain of Responsibility | ☐ | ☐ | ☐ | ☐ | ☐ |
| 14 | Abstract Factory | ☐ | ☐ | ☐ | ☐ | ☐ |
| 15 | Singleton | ☐ | ☐ | ☐ | ☐ | ☐ |
| 16 | Iterator | ☐ | ☐ | ☐ | ☐ | ☐ |
| 17 | Bridge | ☐ | ☐ | ☐ | ☐ | ☐ |
| 18 | Mediator | ☐ | ☐ | ☐ | ☐ | ☐ |
| 19 | Memento | ☐ | ☐ | ☐ | ☐ | ☐ |
| 20 | Visitor | ☐ | ☐ | ☐ | ☐ | ☐ |
| 21 | Prototype | ☐ | ☐ | ☐ | ☐ | ☐ |
| 22 | Flyweight | ☐ | ☐ | ☐ | ☐ | ☐ |
| 23 | Interpreter | ☐ | ☐ | ☐ | ☐ | ☐ |
| 90–92 | Comparisons | ☐ | — | — | ☐ | ☐ |
| Capstone | mini-gateway | — | ☐ | ☐ | ☐ | ☐ |
