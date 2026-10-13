# design_pattern

The 23 Gang of Four design patterns, explained from their **force** (what varies, what must stay still) and written in **modern C++20**. Every example comes from one domain: a video-surveillance edge gateway, with cameras, streams, recording, detection events and relays.

- **Plan & progress:** [LEARNING-PLAN.md](LEARNING-PLAN.md)
- **Start here:** [docs/00-foundations.md](docs/00-foundations.md)

## Build & run

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure     # every example self-checks
./build/examples/01-strategy/pattern
```

Requires a C++20 compiler (tested with g++ 13) and CMake ≥ 3.20.

## Patterns, in learning order

| # | Pattern | Family | Gateway example | Status |
|---|---|---|---|---|
| 00 | [Foundations](docs/00-foundations.md) | — | One camera, four kinds of polymorphism; dependency inversion | ✅ |
| 01 | [Strategy](docs/01-strategy.md) | Behavioral | Recording policy: continuous / scheduled / motion | ✅ |
| 02 | [Observer](docs/02-observer.md) | Behavioral | Detection event fan-out | ✅ |
| 03 | [Adapter](docs/03-adapter.md) | Structural | Vendor camera APIs → one interface | ✅ |
| 04 | Facade | Structural | `recorder.start()` over the media stack | ☐ |
| 05 | Decorator | Structural | Event sink + retry / rate-limit / metrics | ☐ |
| 06 | Factory Method | Creational | Recorder picks its segment writer | ☐ |
| 07 | Builder | Creational | Media pipeline assembly | ☐ |
| 08 | Command | Behavioral | Relay / PTZ actions: queue, log, undo | ☐ |
| 09 | Template Method | Behavioral | Camera onboarding skeleton | ☐ |
| 10 | State | Behavioral | Camera connection lifecycle | ☐ |
| 11 | Composite | Structural | Site → zone → device tree | ☐ |
| 12 | Proxy | Structural | Lazy / caching / guarded camera | ☐ |
| 13 | Chain of Responsibility | Behavioral | Event filter chain | ☐ |
| 14 | Abstract Factory | Creational | ARM appliance vs x86 VM platform family | ☐ |
| 15 | Singleton | Creational | Logger, then dependency injection | ☐ |
| 16 | Iterator | Behavioral | Recorded segments as a C++20 range | ☐ |
| 17 | Bridge | Structural | Alert kind × transport | ☐ |
| 18 | Mediator | Behavioral | Alarm controller | ☐ |
| 19 | Memento | Behavioral | Config snapshot & rollback | ☐ |
| 20 | Visitor | Behavioral | Operations over the device tree; `std::visit` | ☐ |
| 21 | Prototype | Creational | Clone a camera template | ☐ |
| 22 | Flyweight | Structural | Shared bounding-box class metadata | ☐ |
| 23 | Interpreter | Behavioral | Alert rule DSL | ☐ |

## By GoF family

- **Creational:** Factory Method (06), Abstract Factory (14), Builder (07), Prototype (21), Singleton (15)
- **Structural:** Adapter (03), Bridge (17), Composite (11), Decorator (05), Facade (04), Flyweight (22), Proxy (12)
- **Behavioral:** Chain of Responsibility (13), Command (08), Interpreter (23), Iterator (16), Mediator (18), Memento (19), Observer (02), State (10), Strategy (01), Template Method (09), Visitor (20)

## Layout

```
common/      shared domain types + CHECK macros
docs/        one explanation per pattern (same 9 sections each)
examples/    NN-<pattern>/ naive.cpp · pattern.cpp · modern.cpp
notes/       my own notes, one per pattern
```
