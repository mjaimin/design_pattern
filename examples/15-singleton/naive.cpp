// 15 Singleton — the naive version: a plain global object.
//
// Every component logs through `g_log`, a namespace-scope Logger. In a real
// codebase it is declared `extern Logger g_log;` in a header and defined in
// one .cpp file.
//
// Where it hurts:
//   1. Static initialisation order across translation units is unspecified.
//      If another file's global (say a CameraRegistry) logs from its
//      constructor, it may run before g_log is constructed — undefined
//      behaviour that depends on link order. (Not reproduced here: inside one
//      .cpp, order is definition order and therefore well defined.)
//   2. Nothing enforces "one": anybody can declare a second Logger and split
//      the log in two.
//   3. Every component depends on the global by name. A test cannot give
//      CameraManager its own logger, and state leaks between tests.
// The usual "fix" for 1 and 2 is the Singleton pattern — see pattern.cpp.

#include <string>
#include <vector>

#include "check.hpp"

class Logger {
public:
    void info(const std::string& msg) { lines_.push_back(msg); }
    int lines() const { return static_cast<int>(lines_.size()); }

private:
    std::vector<std::string> lines_;
};

Logger g_log;  // the global

class CameraManager {
public:
    void add_camera(const std::string& id) {
        ++count_;
        g_log.info("camera added: " + id);  // hidden dependency
    }

private:
    int count_ = 0;
};

class Recorder {
public:
    void start(const std::string& id) { g_log.info("recording started: " + id); }
};

// Boot scenario: 3 cameras, 2 recordings → 5 log lines.
void boot() {
    CameraManager cams;
    Recorder rec;
    for (auto id : {"cam-1", "cam-2", "cam-3"}) cams.add_camera(id);
    rec.start("cam-1");
    rec.start("cam-2");
}

int main() {
    boot();
    CHECK_EQ(g_log.lines(), 5);

    // "Test #2" starts from whatever test #1 left behind.
    boot();
    CHECK_EQ(g_log.lines(), 10);

    // Nothing stops a second "global" logger.
    Logger another;
    another.info("this line is lost to anyone reading g_log");
    CHECK_EQ(g_log.lines(), 10);

    std::cout << "15-singleton/naive: boot logs 5 lines; second run sees 10 (shared global state)\n";
}
