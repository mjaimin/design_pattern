// 15 Singleton — the rewrite: dependency injection, one instance by choice.
//
// "Exactly one Logger" is a fact about how the *program* is assembled, not
// about the Logger class. So the decision moves to main() — the composition
// root — which creates one Logger and passes it to whoever needs it.
//
//   - The dependency is visible: CameraManager(Logger&) says what it needs.
//   - Tests create their own Logger: no shared state, no reset hook.
//   - Where the output goes is a constructor argument (std::ostream&), so a
//     test captures into a stringstream and production writes to std::clog —
//     no interface needed, because the variation is just "which stream".
//   - Lifetime is ordinary scope: the Logger outlives the components because
//     main() declares it first. No static destruction-order puzzle.

#include <algorithm>
#include <sstream>
#include <string>

#include "check.hpp"

class Logger {
public:
    explicit Logger(std::ostream& out) : out_(out) {}
    void info(const std::string& msg) { out_ << msg << '\n'; }

private:
    std::ostream& out_;
};

class CameraManager {
public:
    explicit CameraManager(Logger& log) : log_(log) {}  // dependency is explicit
    void add_camera(const std::string& id) {
        ++count_;
        log_.info("camera added: " + id);
    }

private:
    Logger& log_;
    int count_ = 0;
};

class Recorder {
public:
    explicit Recorder(Logger& log) : log_(log) {}
    void start(const std::string& id) { log_.info("recording started: " + id); }

private:
    Logger& log_;
};

void boot(Logger& log) {
    CameraManager cams(log);
    Recorder rec(log);
    for (auto id : {"cam-1", "cam-2", "cam-3"}) cams.add_camera(id);
    rec.start("cam-1");
    rec.start("cam-2");
}

int count_lines(const std::string& s) { return static_cast<int>(std::ranges::count(s, '\n')); }

int main() {
    // Test #1 and test #2 each get their own logger: no leak, no reset hook.
    std::ostringstream capture1;
    Logger log1(capture1);
    boot(log1);
    CHECK_EQ(count_lines(capture1.str()), 5);

    std::ostringstream capture2;
    Logger log2(capture2);
    boot(log2);
    CHECK_EQ(count_lines(capture2.str()), 5);

    CHECK(capture1.str().starts_with("camera added: cam-1\n"));

    // Production: main() makes exactly one, pointing at the real stream.
    //   Logger log(std::clog);
    //   boot(log);

    std::cout << "15-singleton/modern: each boot logs 5 lines into its own injected logger\n";
}
