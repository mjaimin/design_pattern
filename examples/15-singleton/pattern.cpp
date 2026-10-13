// 15 Singleton — classic form, written the C++11 way ("Meyers singleton").
//
// Force (as usually stated): exactly one Logger must exist, reachable from
//        anywhere, created before its first use.
//
// Roles
//   Singleton : Logger — private constructor, no copies, and a static
//               instance() that returns the one object
//
// What it fixes compared with naive.cpp:
//   - Construction on first use: the function-local static is initialised
//     the first time instance() runs, so no other global can see it
//     unconstructed (cures the static initialisation order problem).
//   - That initialisation is thread-safe since C++11 ("magic statics").
//   - A second Logger cannot be created: the constructor is private.
//
// What it does NOT fix — and this is why it is usually an anti-pattern:
//   - It is still a global. Components reach it by name, so the dependency
//     is invisible in their constructors and signatures.
//   - State still leaks between tests (shown below); you end up adding a
//     reset_for_tests() that production code must never call.
//   - You cannot give one component a different logger (a test capture, a
//     per-camera log file) without changing that component.
//   - Destruction order: a static object whose destructor logs may run
//     after the Logger has been destroyed.

#include <string>
#include <vector>

#include "check.hpp"

class Logger {
public:
    static Logger& instance() {
        static Logger the_one;  // constructed on first call, thread-safe
        return the_one;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void info(const std::string& msg) { lines_.push_back(msg); }
    int lines() const { return static_cast<int>(lines_.size()); }

    // The escape hatch every singleton grows: tests need a clean slate.
    void reset_for_tests() { lines_.clear(); }

private:
    Logger() = default;
    std::vector<std::string> lines_;
};

class CameraManager {
public:
    void add_camera(const std::string& id) {
        ++count_;
        Logger::instance().info("camera added: " + id);  // still a hidden dependency
    }

private:
    int count_ = 0;
};

class Recorder {
public:
    void start(const std::string& id) { Logger::instance().info("recording started: " + id); }
};

void boot() {
    CameraManager cams;
    Recorder rec;
    for (auto id : {"cam-1", "cam-2", "cam-3"}) cams.add_camera(id);
    rec.start("cam-1");
    rec.start("cam-2");
}

int main() {
    // Same numbers as naive.cpp.
    boot();
    CHECK_EQ(Logger::instance().lines(), 5);

    boot();
    CHECK_EQ(Logger::instance().lines(), 10);  // the leak is still there

    // Uniqueness is now enforced (does not compile):
    //   Logger another;            // error: constructor is private
    //   Logger copy = Logger::instance();  // error: copy is deleted
    CHECK(&Logger::instance() == &Logger::instance());

    Logger::instance().reset_for_tests();
    boot();
    CHECK_EQ(Logger::instance().lines(), 5);

    std::cout << "15-singleton/pattern: boot logs 5 lines; second run sees 10 until reset_for_tests()\n";
}
