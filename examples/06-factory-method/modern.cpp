// 06 Factory Method — two modern C++ forms of "let someone else decide what to create".
//
// A. Injected factory function  (composition instead of a creator subclass)
//    pattern.cpp needs one subclass per format, and each subclass exists *only*
//    to override create_writer(). That is a parallel hierarchy that buys
//    nothing. Passing the creation decision in as a std::function keeps the
//    core idea — Recorder never names a concrete writer — without subclasses.
//    Wins when: the creator varies in nothing else but the product.
//
// B. Registry  (a "simple factory" keyed by a runtime string)
//    The format usually comes from config ("container": "mkv"). A map from
//    name to factory function lets plugins register formats at startup; the
//    core never changes. Not a GoF pattern by itself, but it is what a
//    config-driven gateway actually ships, and it feeds A.
//    Wins when: the choice is data (config file, API request), not code.

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "segment_writers.hpp"

using WriterFactory = std::function<std::unique_ptr<SegmentWriter>(int index)>;

// ---------------------------------------------------------------------------
// A. Recorder with an injected factory — no subclasses
// ---------------------------------------------------------------------------
class Recorder {
public:
    Recorder(WriterFactory make_writer, int frames_per_segment)
        : make_writer_(std::move(make_writer)), frames_per_segment_(frames_per_segment) {}

    void on_frame(const gw::Frame& f) {
        if (!current_ || frames_in_segment_ == frames_per_segment_) rotate();
        current_->write(f);
        ++frames_in_segment_;
    }

    void stop() { close_current(); }

    const std::vector<std::string>& files() const { return files_; }
    std::size_t total_bytes() const { return total_bytes_; }

private:
    void rotate() {
        close_current();
        current_ = make_writer_(next_index_++);
        frames_in_segment_ = 0;
    }

    void close_current() {
        if (!current_) return;
        current_->finish();
        total_bytes_ += current_->bytes();
        files_.push_back(current_->file_name());
        current_.reset();
    }

    WriterFactory make_writer_;
    int frames_per_segment_;
    int frames_in_segment_ = 0;
    int next_index_ = 0;
    std::unique_ptr<SegmentWriter> current_;
    std::vector<std::string> files_;
    std::size_t total_bytes_ = 0;
};

// ---------------------------------------------------------------------------
// B. Registry: format name → factory. Plugins add entries; the core only looks up.
// ---------------------------------------------------------------------------
class WriterRegistry {
public:
    void add(std::string format, WriterFactory make) { makers_[std::move(format)] = std::move(make); }

    // Empty std::function when unknown: the caller decides how to report a bad config.
    WriterFactory find(const std::string& format) const {
        const auto it = makers_.find(format);
        return it == makers_.end() ? WriterFactory{} : it->second;
    }

private:
    std::map<std::string, WriterFactory> makers_;
};

WriterRegistry builtin_formats() {
    WriterRegistry r;
    r.add("mp4", [](int i) { return std::make_unique<Mp4Writer>(i); });
    r.add("mkv", [](int i) { return std::make_unique<MkvWriter>(i); });
    r.add("raw", [](int i) { return std::make_unique<RawH264Writer>(i); });
    return r;
}

std::size_t run_day(Recorder& r) {
    for (const auto& f : one_day()) r.on_frame(f);
    r.stop();
    return r.total_bytes();
}

int main() {
    const WriterRegistry formats = builtin_formats();

    // Config-driven: the string would come from the camera's recording settings.
    Recorder mp4(formats.find("mp4"), kFramesPerSegment);
    CHECK_EQ(run_day(mp4), kMp4Bytes);
    CHECK(mp4.files() == kMp4Files);

    Recorder mkv(formats.find("mkv"), kFramesPerSegment);
    CHECK_EQ(run_day(mkv), kMkvBytes);

    Recorder raw(formats.find("raw"), kFramesPerSegment);
    CHECK_EQ(run_day(raw), kRawBytes);

    CHECK(!formats.find("avi"));  // unknown format → reject the config, don't crash later

    std::cout << "06-factory-method/modern: injected factory + registry match pattern.cpp\n";
}
