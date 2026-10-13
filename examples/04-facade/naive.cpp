// 04 Facade — the naive version.
//
// Two different callers start recordings: the HTTP API handler (operator
// clicks "record") and the scheduler (22:00 night recording). Each one drives
// the four media components itself, in the right order, with rollback.
//
// Where it hurts:
//   - the 6-step start sequence and its rollback are written twice; the next
//     fix (e.g. "release storage if DESCRIBE fails too") must be made twice,
//     and one copy will be missed
//   - both callers include and construct every media component; a change to
//     SegmentMuxer::open() ripples into the API layer and the scheduler
//   - callers must know subsystem rules (configure the depayloader *before*
//     opening the muxer; release storage on failure) that aren't their job
// Duplicate Code + Inappropriate Intimacy between high-level callers and
// low-level components.

#include <map>

#include "check.hpp"
#include "media_stack.hpp"

// Per-camera live objects, owned by... whoever started it. Here: a global
// both callers share, because neither owns recording.
struct LiveSession {
    RtspSession rtsp;
    Depayloader depay;
    SegmentMuxer mux;
};
std::map<std::string, LiveSession> g_live;

// ---- Caller 1: HTTP API handler ------------------------------------------------
bool api_handle_start(CallLog& log, StorageAllocator& storage, const std::string& cam,
                      const std::string& url) {
    auto path = storage.reserve(cam);
    if (!path) return false;
    LiveSession s{RtspSession(log), Depayloader(log), SegmentMuxer(log)};
    if (!s.rtsp.connect(url)) {
        storage.release(*path);
        return false;
    }
    const auto codec = s.rtsp.describe();
    s.depay.configure(codec);
    s.mux.open(*path, codec);
    s.rtsp.play();
    g_live.emplace(cam, std::move(s));
    return true;
}

void api_handle_stop(const std::string& cam) {
    auto it = g_live.find(cam);
    if (it == g_live.end()) return;
    it->second.rtsp.teardown();
    it->second.mux.close();
    g_live.erase(it);
}

// ---- Caller 2: scheduler — the same sequence, copied --------------------------
bool scheduler_start_night_recording(CallLog& log, StorageAllocator& storage,
                                     const std::string& cam, const std::string& url) {
    auto path = storage.reserve(cam);
    if (!path) return false;
    LiveSession s{RtspSession(log), Depayloader(log), SegmentMuxer(log)};
    if (!s.rtsp.connect(url)) {
        storage.release(*path);
        return false;
    }
    const auto codec = s.rtsp.describe();
    s.depay.configure(codec);
    s.mux.open(*path, codec);
    s.rtsp.play();
    g_live.emplace(cam, std::move(s));
    return true;
}

int main() {
    CallLog log;
    StorageAllocator storage(log);

    CHECK(api_handle_start(log, storage, "cam-1", "rtsp://10.0.0.11/stream1"));
    CHECK(scheduler_start_night_recording(log, storage, "cam-2", "rtsp://10.0.0.12/stream1"));
    CHECK(!api_handle_start(log, storage, "cam-3", "rtsp://10.0.0.99/stream1"));
    api_handle_stop("cam-1");

    CHECK_EQ(log.size(), kExpectedCalls);
    CHECK_EQ(g_live.size(), kExpectedActive);
    CHECK_EQ(log[14], std::string("storage.release /var/rec/cam-3"));  // rollback happened

    std::cout << "04-facade/naive: calls=" << log.size() << " active=" << g_live.size() << "\n";
}
