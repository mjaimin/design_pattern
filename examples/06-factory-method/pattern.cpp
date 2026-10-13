// 06 Factory Method — classic form (a virtual creation hook in the creator).
//
// Force: the container format of recorded segments varies with the kind of
//        recorder (MP4 archive, MKV archive, raw elementary-stream dump for
//        forensics); the segmenting algorithm — open, write N frames, finalise,
//        rotate — must live in one place and never name a concrete format.
//
// Roles
//   Creator           : Recorder                     — owns the algorithm that *uses* the product
//   Factory method    : Recorder::create_writer(int) — the one virtual "new"
//   Concrete creators : Mp4Recorder, MkvRecorder, RawRecorder
//   Product           : SegmentWriter
//   Concrete products : Mp4Writer, MkvWriter, RawH264Writer   (segment_writers.hpp)
//
// Difference from naive.cpp's "simple factory": Recorder is not a factory.
// Its job is recording; it merely needs an object it cannot name, so it
// defers that single decision to a subclass. Recorder now compiles without
// knowing any concrete writer exists.

#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "segment_writers.hpp"

// ---- Creator ----------------------------------------------------------------
class Recorder {
public:
    explicit Recorder(int frames_per_segment) : frames_per_segment_(frames_per_segment) {}
    virtual ~Recorder() = default;

    // The algorithm that uses the product. Written once, format-agnostic.
    void on_frame(const gw::Frame& f) {
        if (!current_ || frames_in_segment_ == frames_per_segment_) rotate();
        current_->write(f);
        ++frames_in_segment_;
    }

    void stop() { close_current(); }

    const std::vector<std::string>& files() const { return files_; }
    std::size_t total_bytes() const { return total_bytes_; }

protected:
    // The factory method. Protected: a hook for subclasses, not public API.
    virtual std::unique_ptr<SegmentWriter> create_writer(int index) = 0;

private:
    void rotate() {
        close_current();
        // Called lazily on the first frame — never from Recorder's constructor,
        // where virtual dispatch would not reach the subclass yet (doc §4).
        current_ = create_writer(next_index_++);
        frames_in_segment_ = 0;
    }

    void close_current() {
        if (!current_) return;
        current_->finish();
        total_bytes_ += current_->bytes();
        files_.push_back(current_->file_name());
        current_.reset();
    }

    int frames_per_segment_;
    int frames_in_segment_ = 0;
    int next_index_ = 0;
    std::unique_ptr<SegmentWriter> current_;
    std::vector<std::string> files_;
    std::size_t total_bytes_ = 0;
};

// ---- Concrete creators: each overrides only the creation decision -----------
class Mp4Recorder final : public Recorder {
public:
    using Recorder::Recorder;

protected:
    std::unique_ptr<SegmentWriter> create_writer(int index) override {
        return std::make_unique<Mp4Writer>(index);
    }
};

class MkvRecorder final : public Recorder {
public:
    using Recorder::Recorder;

protected:
    std::unique_ptr<SegmentWriter> create_writer(int index) override {
        return std::make_unique<MkvWriter>(index);
    }
};

class RawRecorder final : public Recorder {
public:
    using Recorder::Recorder;

protected:
    std::unique_ptr<SegmentWriter> create_writer(int index) override {
        return std::make_unique<RawH264Writer>(index);
    }
};

std::size_t run_day(Recorder& r) {
    for (const auto& f : one_day()) r.on_frame(f);
    r.stop();
    return r.total_bytes();
}

int main() {
    // Same numbers as naive.cpp: only the creation decision moved.
    Mp4Recorder mp4(kFramesPerSegment);
    CHECK_EQ(run_day(mp4), kMp4Bytes);
    CHECK(mp4.files() == kMp4Files);

    MkvRecorder mkv(kFramesPerSegment);
    CHECK_EQ(run_day(mkv), kMkvBytes);
    CHECK_EQ(mkv.files().back(), std::string("seg-0002.mkv"));

    RawRecorder raw(kFramesPerSegment);
    CHECK_EQ(run_day(raw), kRawBytes);
    CHECK_EQ(raw.files().size(), 3);

    std::cout << "06-factory-method/pattern: mp4=" << kMp4Bytes << " mkv=" << kMkvBytes
              << " raw=" << kRawBytes << " segments=3\n";
}
