# 15 — Singleton

> Family: **Creational** · Code: [`examples/15-singleton/`](../examples/15-singleton/)
> Run: `./build/examples/15-singleton/naive`, `pattern`, `modern`

**Intent (GoF):** ensure a class has only one instance, and provide a global point of access to it.

> **Read this chapter as "Singleton, and why you'll usually replace it."** The pattern bundles two promises, *only one instance* and *reachable from anywhere*. The first is sometimes a real requirement. The second is a global variable, with all the costs of one. `modern.cpp` keeps the first promise and drops the second.

---

## 1. Force

> **Exactly one** logger must exist in the process, created **before its first use**. Every component (camera manager, recorder, relay controller) must write to **the same** one.

- **What varies:** nothing. That's the tell. Singleton isn't about variation. It's about **lifetime and access**.
- **Who demands "one":** sometimes the hardware (one `/dev/watchdog`, opened once), sometimes consistency (one log stream), often just convenience.
- **When:** for the whole process lifetime.
- **What stays still:** the call sites, which want to log without being handed anything.

That last point is the trap. "Without being handed anything" means the dependency is hidden, and every problem below follows from that.

## 2. Naive code and where it hurts

[`naive.cpp`](../examples/15-singleton/naive.cpp): a plain namespace-scope global.

```cpp
Logger g_log;                              // extern'd from a header in real code

class CameraManager {
    void add_camera(const std::string& id) { g_log.info("camera added: " + id); }
};
```

1. **Static initialisation order fiasco.** The order in which globals in *different* `.cpp` files are constructed is unspecified. If a global `CameraRegistry` in another file logs from its constructor, it can run before `g_log` exists. The result is undefined behaviour, and it changes with link order. (This can't be reproduced inside one file, where definition order decides.)
2. **Nothing enforces "one".** `Logger another;` compiles and silently splits the log.
3. **Hidden dependency, shared state.** Every component names the global. Running the boot scenario twice gives 10 lines, not 5: test #2 sees test #1's state.

Smells, by name:
- **Global data:** mutable state reachable and changeable from anywhere.
- **Hidden dependency:** `CameraManager`'s constructor says it needs nothing, which isn't true.

## 3. Structure

```mermaid
classDiagram
    class Logger {
        -Logger()
        -lines_
        +instance()$ Logger&
        +info(string)
        Logger(const Logger&) = delete
    }
    CameraManager ..> Logger : Logger::instance()
    Recorder ..> Logger : Logger::instance()
```

| Role | In this example | Responsibility |
|---|---|---|
| **Singleton** | `Logger` | Private constructor and deleted copy, so no one else can make one. `static instance()` returns the single object. |
| **Clients** | `CameraManager`, `Recorder` | Call `Logger::instance()` wherever they need it. |

## 4. Gateway example walkthrough

[`pattern.cpp`](../examples/15-singleton/pattern.cpp): the C++11 "Meyers singleton".

```cpp
class Logger {
public:
    static Logger& instance() {
        static Logger the_one;          // initialised on first call; thread-safe since C++11
        return the_one;
    }
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    void info(const std::string& msg);
    void reset_for_tests();             // the hook every singleton eventually grows
private:
    Logger() = default;
};
```

What it fixes compared with `naive.cpp`:

- **Initialisation order.** A function-local static is constructed the first time control passes through it, so any caller of `instance()` gets a constructed object, even from another global's constructor.
- **Thread-safe construction.** Since C++11, concurrent first calls block until one of them finishes initialisation ("magic statics"). No double-checked locking is needed.
- **Uniqueness.** With a private constructor and deleted copy, `Logger another;` doesn't compile.

What it does **not** fix (the test proves it):

- **State still leaks.** The second boot still sees 10 lines, and the test needs `reset_for_tests()`, a method production code must never call.
- **Still hidden.** `CameraManager` still reaches out by name. You can't give it a different logger (a capture in a test, a per-camera file) without editing it.
- **Destruction order.** Function-local statics are destroyed in reverse order of construction at exit. A static whose destructor logs can run after the Logger is gone.
- **Thread-safe construction ≠ thread-safe use.** `info()` still needs its own locking if called from several threads.

## 5. Modern C++ form

[`modern.cpp`](../examples/15-singleton/modern.cpp): **dependency injection**. "One logger" becomes a decision made by `main()`, not a property of the class.

```cpp
class Logger {
public:
    explicit Logger(std::ostream& out);        // where output goes is an argument
    void info(const std::string& msg);
};

class CameraManager {
public:
    explicit CameraManager(Logger& log);       // the dependency is now visible
};

int main() {
    Logger log(std::clog);                     // exactly one, by construction in the composition root
    CameraManager cams(log);
    Recorder rec(log);
}
```

- Each test creates its own `Logger` over an `std::ostringstream`, and both boots see exactly 5 lines. No reset hook.
- No interface is needed. The thing that varies is only *which stream*, so `std::ostream&` is the seam.
- Lifetime is plain scope. `log` is declared first in `main()`, so it outlives everything that uses it.

| Form | Pick it when | Cost |
|---|---|---|
| Constructor injection | **Almost always.** Application components, anything you want to test. | Constructors list their dependencies, and threading a logger through deep call chains is visible work. That visibility is the point. |
| Meyers singleton | A genuinely process-wide resource **and** callers that can't be given parameters: C callbacks, signal handlers, code in static initialisers | Global state, hidden dependency, test-reset hooks |
| Namespace-scope global | Immutable constants (`constexpr`/`constinit`), where there's no state to leak | Unspecified cross-TU initialisation order, if not constant-initialised |

## 6. When NOT to use it

- **You need "one" only because passing it around is tedious.** That's the main abuse. Pass it in and accept the visible parameter.
- **The object has mutable state that tests touch.** Any singleton with state turns your test suite order-dependent.
- **You might ever need two:** two log files, two configs (one per tenant), two device managers in one integration test. Singleton makes "two" a rewrite.
- **"There's only one in production"** is a statement about deployment. Put it in `main()`, not in the class.

Legitimate cases are narrow: an object that must be reachable from code you can't add parameters to (a POSIX signal handler, a C library callback with no user-data pointer), or the standard library's own process-wide objects below.

## 7. Confused with

| Pattern | Difference in one line |
|---|---|
| **Monostate** | All data members are `static` but instances are ordinary. Callers write `Logger{}` and share hidden state. It's the same global under a disguise. |
| **Abstract Factory** (14) / **Facade** (04) | Often *implemented* as singletons, but nothing in either pattern requires it. Create one in `main()` and pass it. |
| **Flyweight** (22) | Flyweight shares **many** immutable instances keyed by intrinsic state. Singleton has **one** instance, usually mutable. |
| **Dependency injection** | Same "one instance" outcome, but the decision lives in the composition root and the dependency is visible. This is the replacement, not a competitor. |

## 8. Seen in the wild

- **`std::cout`, `std::cerr`, `std::clog`:** process-wide stream objects. Implementations guarantee they're constructed before any user code can use them, typically with the "nifty counter" technique (`std::ios_base::Init`).
- **`std::pmr::new_delete_resource()` / `std::pmr::null_memory_resource()`:** return pointers to single, process-wide memory resource objects.
- **Logging libraries with a "default logger":** spdlog's `spdlog::default_logger()` is backed by a process-wide registry. The same libraries also let you create and pass logger objects explicitly, which is the injectable path.

## 9. Exercise and checkpoint

**Exercise**
1. In `pattern.cpp`, add a static `CameraRegistry` object whose destructor logs `"registry shutting down"` through `Logger::instance()`. Work out which object is destroyed first, and whether the log call is safe. (Hint: whichever static finished construction *last* is destroyed *first*.)
2. In `modern.cpp`, give `Recorder` its own logger writing to a second stream, while `CameraManager` keeps the shared one. What would the same change cost in `pattern.cpp`?
3. Make `Logger::info` safe to call from several threads in `modern.cpp` (a `std::mutex` member). Note that this is unrelated to how many instances exist.

**Checkpoint**: answer from memory:
- Which problem does a function-local static solve that a namespace-scope global doesn't?
- Name two problems the Meyers singleton does **not** solve.
- Your watchdog device can be opened only once per process. Does that require the Singleton pattern? Where would you enforce "once" instead?
