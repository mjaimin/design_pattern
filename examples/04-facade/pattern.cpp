// 04 Facade — one simple door in front of the recording subsystem.
//
// Force: the media stack (storage, RTSP, depayloader, muxer) has ordering
//        rules and rollback paths that change as the stack evolves; the
//        callers (API handler, scheduler, CLI tool) want "start/stop
//        recording camera X" and must not change when the stack does.
//
// Roles
//   Facade     : RecordingService   — start(), stop(), active(); owns the sequence
//   Subsystem  : StorageAllocator, RtspSession, Depayloader, SegmentMuxer
//                (unchanged, still public — the facade doesn't hide them by force)
//   Clients    : api_handle_start/stop, scheduler_start_night_recording
//
// Compare with naive.cpp: the start sequence and its rollback exist once.
// The callers shrink to one line each and include one header's worth of API.

#include <map>

#include "check.hpp"
#include "media_stack.hpp"

// ---- Facade -------------------------------------------------------------------
class RecordingService {
public:
    explicit RecordingService(CallLog& log) : log_(log), storage_(log) {}

    bool start(const std::string& cam, const std::string& url) {
        auto path = storage_.reserve(cam);
        if (!path) return false;
        Session s{RtspSession(log_), Depayloader(log_), SegmentMuxer(log_)};
        if (!s.rtsp.connect(url)) {
            storage_.release(*path);  // rollback lives in exactly one place
            return false;
        }
        const auto codec = s.rtsp.describe();
        s.depay.configure(codec);  // ordering rule: depay before mux
        s.mux.open(*path, codec);
        s.rtsp.play();
        live_.emplace(cam, std::move(s));
        return true;
    }

    void stop(const std::string& cam) {
        auto it = live_.find(cam);
        if (it == live_.end()) return;
        it->second.rtsp.teardown();
        it->second.mux.close();
        live_.erase(it);
    }

    std::size_t active() const { return live_.size(); }

private:
    struct Session {
        RtspSession rtsp;
        Depayloader depay;
        SegmentMuxer mux;
    };
    CallLog& log_;
    StorageAllocator storage_;
    std::map<std::string, Session> live_;  // the facade owns recording state
};

// ---- Clients: one line each, no media types in sight ----------------------------
bool api_handle_start(RecordingService& rec, const std::string& cam, const std::string& url) {
    return rec.start(cam, url);
}
void api_handle_stop(RecordingService& rec, const std::string& cam) { rec.stop(cam); }
bool scheduler_start_night_recording(RecordingService& rec, const std::string& cam,
                                     const std::string& url) {
    return rec.start(cam, url);
}

int main() {
    CallLog log;
    RecordingService rec(log);

    CHECK(api_handle_start(rec, "cam-1", "rtsp://10.0.0.11/stream1"));
    CHECK(scheduler_start_night_recording(rec, "cam-2", "rtsp://10.0.0.12/stream1"));
    CHECK(!api_handle_start(rec, "cam-3", "rtsp://10.0.0.99/stream1"));
    api_handle_stop(rec, "cam-1");

    // Same subsystem call sequence as naive.cpp.
    CHECK_EQ(log.size(), kExpectedCalls);
    CHECK_EQ(rec.active(), kExpectedActive);
    CHECK_EQ(log[14], std::string("storage.release /var/rec/cam-3"));

    std::cout << "04-facade/pattern: calls=" << log.size() << " active=" << rec.active() << "\n";
}
