#pragma once
// A simulated recording subsystem: four components that must be driven in
// the right order, with rollback on failure. Every call appends to a shared
// log, so naive.cpp and pattern.cpp can prove they drive it identically.
//
//   StorageAllocator  reserve space for a camera's segments, release on failure
//   RtspSession       connect → DESCRIBE (returns codec) → SETUP/PLAY → TEARDOWN
//   Depayloader       turns RTP packets into codec frames; needs the codec
//   SegmentMuxer      writes frames into MP4 segments at a path

#include <optional>
#include <string>
#include <vector>

using CallLog = std::vector<std::string>;

class StorageAllocator {
public:
    explicit StorageAllocator(CallLog& log) : log_(log) {}
    std::optional<std::string> reserve(const std::string& camera) {
        log_.push_back("storage.reserve " + camera);
        return "/var/rec/" + camera;
    }
    void release(const std::string& path) { log_.push_back("storage.release " + path); }

private:
    CallLog& log_;
};

class RtspSession {
public:
    explicit RtspSession(CallLog& log) : log_(log) {}
    bool connect(const std::string& url) {
        log_.push_back("rtsp.connect " + url);
        return url.find("10.0.0.99") == std::string::npos;  // .99 is unreachable
    }
    std::string describe() {
        log_.push_back("rtsp.describe");
        return "H264";
    }
    void play() { log_.push_back("rtsp.play"); }
    void teardown() { log_.push_back("rtsp.teardown"); }

private:
    CallLog& log_;
};

class Depayloader {
public:
    explicit Depayloader(CallLog& log) : log_(log) {}
    void configure(const std::string& codec) { log_.push_back("depay.configure " + codec); }

private:
    CallLog& log_;
};

class SegmentMuxer {
public:
    explicit SegmentMuxer(CallLog& log) : log_(log) {}
    void open(const std::string& path, const std::string& codec) {
        log_.push_back("mux.open " + path + " " + codec);
    }
    void close() { log_.push_back("mux.close"); }

private:
    CallLog& log_;
};

// Expected over the scenario used by both examples:
//   start cam-1 (ok)        6 calls
//   start cam-2 (ok)        6 calls
//   start cam-3 (offline)   3 calls: reserve, connect fails, release
//   stop  cam-1             2 calls
inline constexpr int kExpectedCalls = 6 + 6 + 3 + 2;
inline constexpr int kExpectedActive = 1;  // cam-2 still recording
