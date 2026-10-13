# 91 — Cheat sheet: what varies → pattern

Start from the **force**: what will change, and what must stay still. Find the row, then check the chapter's *When NOT to use* section before reaching for the pattern. If no row matches, write the direct code.

## Force → pattern → C++ form

| What varies / the problem | Pattern | Preferred C++ form | Ch. |
|---|---|---|---|
| One algorithm, chosen per instance at runtime | Strategy | `unique_ptr<Interface>` if several ops; `std::function` if one op; template policy if fixed at build time | [01](01-strategy.md) |
| Number and identity of reactors to an event | Observer | `std::function` subscribers + RAII subscription handle (or `weak_ptr` listeners) | [02](02-observer.md) |
| A third-party interface that doesn't match yours | Adapter | Class implementing your interface, holding the foreign object by value | [03](03-adapter.md) |
| A messy subsystem callers shouldn't see | Facade | Plain class with a few methods. Usually no interface needed. | [04](04-facade.md) |
| Optional behaviours stacked in any combination | Decorator | `unique_ptr` wrapping the same interface; or composed lambdas for one-op cases | [05](05-decorator.md) |
| Which product a creator makes, decided by its subclass | Factory Method | Protected virtual `make_*()` returning `unique_ptr`; or an injected `std::function` factory | [06](06-factory-method.md) |
| Complex, step-by-step construction with validation | Builder | Fluent builder returning `*this`, `build()` validates and returns the product (or `std::expected`) | [07](07-builder.md) |
| Actions that must be queued, logged, retried, undone | Command | Interface with `execute()`/`undo()`; `std::function<void()>` if no undo | [08](08-command.md) |
| Steps of a fixed algorithm | Template Method | Non-virtual public method + private/protected virtual steps (NVI); CRTP for static | [09](09-template-method.md) |
| Behaviour as a whole, by lifecycle state | State | State classes + context `transition_to()`; `std::variant` of states for closed sets | [10](10-state.md) |
| One object vs a tree of objects, used uniformly | Composite | Base interface + node holding `vector<unique_ptr<Base>>` | [11](11-composite.md) |
| Access to an object: lazy, remote, cached, guarded | Proxy | Same interface, holds or creates the real object; smart pointers are proxies too | [12](12-proxy.md) |
| An ordered set of handlers, any of which may stop a request | Chain of Responsibility | `vector<unique_ptr<Handler>>` or `vector<std::function<bool(T&)>>` walked in order | [13](13-chain-of-responsibility.md) |
| A whole family of products that must match | Abstract Factory | Interface of `make_*()` methods, one implementation per platform, injected | [14](14-abstract-factory.md) |
| Exactly one instance (rarely genuine) | Singleton | Prefer dependency injection; if unavoidable, a function-local static (Meyers) | [15](15-singleton.md) |
| How a collection is laid out, hidden from traversal | Iterator | Standard iterator + `begin()`/`end()`; C++20 ranges/views; `std::generator` (C++23) | [16](16-iterator.md) |
| Two independent axes (abstraction × implementation) | Bridge | Abstraction holds a reference/`unique_ptr` to an implementation interface | [17](17-bridge.md) |
| Many-to-many interactions between peers | Mediator | One coordinator class owning the rules; peers know only the mediator | [18](18-mediator.md) |
| Snapshot/restore without exposing internals | Memento | Opaque nested snapshot type, value semantics, friend-accessed | [19](19-memento.md) |
| New operations over a stable set of types | Visitor | `std::variant` + `std::visit` with `overloaded{}`; classic double dispatch for open hierarchies | [20](20-visitor.md) |
| Creating by copying a configured instance | Prototype | `virtual unique_ptr<Base> clone() const`; plain copy constructor for value types | [21](21-prototype.md) |
| Many objects sharing identical immutable data | Flyweight | Factory returning `const T*`; or a small index into a static table | [22](22-flyweight.md) |
| Sentences of a small language, built at runtime | Interpreter | Recursive-descent parser + node classes, or a `std::variant` AST | [23](23-interpreter.md) |

## The force of each pattern, in one sentence

**Creational**
- **Factory Method:** *which* concrete product gets created varies by subclass, while the code that uses the product stays fixed.
- **Abstract Factory:** a whole **family** of products varies together (per platform or backend), and mixing members of different families must be impossible.
- **Builder:** the **assembly** of a complex object varies in which optional parts are present, and invalid combinations must be rejected before the object exists.
- **Prototype:** new objects should **start as copies of a configured instance**, because constructing and configuring from scratch is costly or the concrete type isn't known to the caller.
- **Singleton:** there must be **exactly one** instance with one access point. Usually this is a disguised global, and injection is the better answer.

**Structural**
- **Adapter:** a **foreign interface** varies by vendor or library, and your code's interface must not.
- **Bridge:** **two dimensions** vary independently, and you want N + M classes, not N × M.
- **Composite:** clients must treat **one item and a tree of items** the same way.
- **Decorator:** **optional responsibilities** vary per instance and must combine freely at runtime without a subclass per combination.
- **Facade:** a **subsystem's complexity** should change freely behind **one simple entry point** that callers depend on.
- **Flyweight:** **object count** grows while the **shared, immutable part** stays small, so memory must scale with the second.
- **Proxy:** **access** to an object varies (lazy, remote, cached, guarded), but its **interface** must not.

**Behavioral**
- **Chain of Responsibility:** the **set and order of handlers** varies, and the sender must not know which one handles or stops the request.
- **Command:** **requests** must become **data**: stored, queued, logged, retried or undone, separately from whoever issued them.
- **Interpreter:** **sentences** (rules) vary at runtime and are written as text, while the **grammar** stays small and stable.
- **Iterator:** the **storage layout** varies, and traversal code must not.
- **Mediator:** **interaction rules** between many peers change often and must live in one place, not spread across the peers.
- **Memento:** an object's **state must be captured and restored** without exposing its internals.
- **Observer:** the **set of reactors** to an event varies, and the source must not know them.
- **State:** **behaviour changes wholesale** with internal state, and transitions follow rules the object owns.
- **Strategy:** **one algorithm** varies per instance and is chosen from outside, while the context that runs it stays fixed.
- **Template Method:** the **steps** vary and the **skeleton** (their order and invariants) must not.
- **Visitor:** **operations** keep being added over a **stable set of types**.

## Picking the C++ mechanism

The forms above map onto the four kinds of polymorphism from [00 §4](00-foundations.md#4-four-kinds-of-polymorphism-in-c):

| Situation | Mechanism |
|---|---|
| Chosen at runtime, open set of types, several operations | `virtual` interface + `unique_ptr` |
| Chosen at runtime, one operation | `std::function` / lambda |
| Chosen at build or config time, hot path | Template parameter constrained by a concept |
| Closed set of types, operations keep growing | `std::variant` + `std::visit` |

## Before you apply any of them

1. **Two real variants exist today**, not one plus a hypothetical.
2. **The source of variation has a name**: a requirement, a vendor, a platform, an operator setting.
3. **The direct code is genuinely worse.** Sketch it first.

See [92 — Anti-patterns](92-anti-patterns.md) for what happens when these checks are skipped.
