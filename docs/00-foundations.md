# 00 — Foundations

> Code: [`examples/00-foundations/`](../examples/00-foundations/) · run `./build/examples/00-foundations/polymorphism` and `./build/examples/00-foundations/dependency_inversion`

Every one of the 23 GoF patterns is a reusable answer to one question:

> **What is going to change here, and what must stay still while it changes?**

That pair is the **force** (or *axis of change*). This repo explains every pattern starting from its force, never from its class diagram. If you can't name a force, you don't need a pattern; write the direct code.

The patterns are built from four ideas: **program to an interface**, **composition over inheritance**, **dependency inversion**, and **polymorphism**. In C++, polymorphism comes in four distinct forms. This chapter covers each idea once, so later chapters can just name them.

---

## 1. Program to an interface, not an implementation

A call site should depend on **what an object can do**, not on **which class it is**.

```cpp
void start_stream(VendorACamera& cam);   // welded to one vendor
void start_stream(Camera& cam);          // works for every Camera, including ones written later
```

The second signature is the precondition for almost every pattern. Strategy, State, Observer, Adapter, Decorator, Proxy, Composite and Command all work by handing a caller an interface while the concrete class behind it changes.

**Interface ≠ `virtual`.** In C++, "interface" means *the set of operations the caller relies on*. An abstract base class is one way to express it, a concept is another, and a `std::function` signature is a third. See §4.

## 2. Composition over inheritance

Two ways to give `Recorder` different behaviours:

```
Inheritance                            Composition
───────────                            ───────────
        Recorder                       Recorder ──has-a──▶ RecordingPolicy
       /    |    \                                          ▲     ▲     ▲
Continuous Motion Scheduled            Continuous  Motion  Scheduled
Recorder   Recorder Recorder
```

| | Inheritance (*is-a*) | Composition (*has-a*) |
|---|---|---|
| Binding | Fixed when the object is created | Can be swapped at runtime |
| Combining two axes (policy × storage) | N × M subclasses | N + M parts |
| Coupling | Subclass sees base internals (fragile base class) | Only the part's interface |
| Testing | Need the whole hierarchy | Inject a fake part |

Inheritance is still right for **genuine subtyping**, where the derived type really is substitutable for the base (Liskov). It's also right when the base supplies a fixed skeleton, which is Template Method in chapter 09. Use it for *types*, not for *reusing code*.

## 3. The SOLID principles the patterns rest on

Only the parts that matter for patterns:

| Principle | One line | Patterns that serve it |
|---|---|---|
| **S**ingle Responsibility | A class has one reason to change | Strategy, Command, Visitor pull a responsibility out |
| **O**pen/Closed | Add behaviour by adding code, not editing working code | Strategy, Decorator, Observer, Chain, Visitor |
| **L**iskov Substitution | Any subtype works wherever the base is expected, no surprises | Every pattern that uses a base class |
| **I**nterface Segregation | Callers shouldn't depend on methods they don't use | Adapter, Facade (narrow views), small Strategy interfaces |
| **D**ependency Inversion | High-level policy and low-level detail both depend on an abstraction owned by the policy | Abstract Factory, Bridge, Observer, any injected collaborator |

### Dependency Inversion, concretely

[`dependency_inversion.cpp`](../examples/00-foundations/dependency_inversion.cpp)

```
Without DI                                  With DI
──────────                                  ───────
ClipExporter ──uses──▶ std::ofstream        ClipExporter ──uses──▶ ClipStore (interface)
 (policy)              (detail)               (policy)                ▲          ▲
                                                               LocalDiskStore  InMemoryStore
```

- The **naming rule** for clips (`cam/date/HHMM.mp4`) is policy. **Where the bytes go** is detail.
- `ClipExporter` defines the `ClipStore` interface it needs and takes one by reference. It's *composed*, not inherited.
- The test uses `InMemoryStore`, with no disk, and checks the naming rule. The same exporter, unchanged, writes real files through `LocalDiskStore`.
- There are **two real implementations**. An interface with one implementation and a "maybe later" is just an extra hop when debugging. Extract the interface when the second implementation arrives. A test fake counts as a real second implementation when you actually need it.

## 4. Four kinds of polymorphism in C++

[`polymorphism.cpp`](../examples/00-foundations/polymorphism.cpp) solves the same problem four times: get the RTSP URL of a camera whose URL layout depends on the vendor.

```cpp
// 1. virtual
struct Camera { virtual std::string rtsp_url() const = 0; };
std::vector<std::unique_ptr<Camera>> cams;           // mixed vendors, runtime

// 2. template + concept
template <HasRtspUrl C> std::string url_of(const C&); // one copy per type, compile time

// 3. std::function (type erasure)
std::vector<std::function<std::string()>> sources;   // any callable, no base class

// 4. std::variant + std::visit
using AnyCamera = std::variant<VendorACam, VendorBCam>; // closed set, checked
```

| | `virtual` | template / concept | `std::function` | `std::variant` |
|---|---|---|---|---|
| Decided at | runtime | **compile time** | runtime | runtime |
| Set of types | open: anyone can add a subclass | open | open | **closed**: listed in the type |
| Needs a common base | yes | no | no | no |
| Heterogeneous container | yes (via pointers) | **no** | yes | yes |
| Storage | usually heap + pointer | by value | by value (may allocate) | by value, no heap |
| Call cost | indirect call through vtable | direct, inlinable | indirect call | branch on index |
| Several operations per type | natural (more virtuals) | natural | awkward: one signature | natural (one visitor per op) |
| Adding a type | add a class | add a class | add a lambda | **edit the variant** |
| Adding an operation | **edit every class** | add a function | new signature | add a visitor |

Use this table to pick which C++ form a pattern should take:

- **Runtime choice, open set, several operations** → `virtual`. This is the classic GoF form.
- **Choice known at build/config time, hot path** → template policy. This is how the STL does Strategy: `Compare`, `Allocator`, `Deleter`.
- **One operation, maybe with captured state** → `std::function` or a lambda.
- **Fixed set of types, operations keep growing** → `std::variant`. This is the modern Visitor (chapter 20).

The last two rows are the **expression problem**. Inheritance makes new *types* cheap and new *operations* expensive. `std::variant` makes it the other way round. Visitor (chapter 20) exists entirely because of this trade-off.

---

## How to read every other chapter

Each pattern doc has the same 9 sections:

1. **Force**: what varies, who makes it vary, and what stays still
2. **Naive code & where it hurts**: `naive.cpp`, plus the concrete change that makes it painful
3. **Structure**: roles and a diagram
4. **Gateway example**: walkthrough of `pattern.cpp`
5. **Modern C++ form**: `modern.cpp`, mapped onto the table in §4
6. **When NOT to use**
7. **Confused with**
8. **Seen in the wild**
9. **Exercise + checkpoint**

`naive.cpp` and `pattern.cpp` always produce **the same numbers**. The pattern changes the code's structure, not its behaviour, and the `CHECK_EQ` lines prove that.

## Exercise

1. In `polymorphism.cpp`, add `VendorCCam` (URL `rtsp://<host>/live/main`) to all four forms. Count how many places you had to edit in each form.
2. Add a second operation, `snapshot_url()`, to all four forms. Count again.
3. Compare the two counts against the last two rows of the §4 table.

## Checkpoint

*Without looking:* you have 3 camera vendors today and expect more. Each camera needs `rtsp_url()`, `snapshot_url()` and `reboot()`. The caller keeps all cameras in one list. Which of the four forms do you choose, and which form would you choose if the vendor list were fixed forever but new operations kept arriving monthly?
