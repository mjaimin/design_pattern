// 06 Factory Method — the naive version ("simple factory" inside the class).
//
// Recorder cuts the incoming stream into segments of N frames. To open each
// segment it needs a SegmentWriter, and it picks one with a switch on an enum.
//
// Where it hurts — try adding a customer-specific container (say, an
// encrypted MP4 variant that lives in a separate plugin library):
//   1. add an enum value to Recorder's header — the core recording library
//   2. #include the new writer into the core library
//   3. add a case to the switch
//   4. rebuild and retest the core, although the segmenting loop did not change
// The class that owns the *algorithm* (rotate every N frames, finalise, record
// the file list) also owns the *catalogue of every format that exists*. Those
// change for different reasons and at different rates (Divergent Change), and
// the core can never be closed for modification.
//
// Note: for a closed set you own, this switch is often fine — see doc §6.

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "check.hpp"
#include "segment_writers.hpp"

enum class Container { Mp4, Mkv, RawH264 };

class Recorder {
public:
    Recorder(Container container, int frames_per_segment)
        : container_(container), frames_per_segment_(frames_per_segment) {}

    void on_frame(const gw::Frame& f) {
        if (!current_ || frames_in_segment_ == frames_per_segment_) rotate();
        current_->write(f);
        ++frames_in_segment_;
    }

    void stop() { close_current(); }

    const std::vector<std::string>& files() const { return files_; }
    std::size_t total_bytes() const { return total_bytes_; }

private:
    // The creation decision, hard-wired to every format the library knows.
    std::unique_ptr<SegmentWriter> create_writer(int index) {
        switch (container_) {
            case Container::Mp4: return std::make_unique<Mp4Writer>(index);
            case Container::Mkv: return std::make_unique<MkvWriter>(index);
            case Container::RawH264: return std::make_unique<RawH264Writer>(index);
        }
        throw std::logic_error("unknown container");
    }

    void rotate() {
        close_current();
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

    Container container_;
    int frames_per_segment_;
    int frames_in_segment_ = 0;
    int next_index_ = 0;
    std::unique_ptr<SegmentWriter> current_;
    std::vector<std::string> files_;
    std::size_t total_bytes_ = 0;
};

std::size_t run_day(Recorder& r) {
    for (const auto& f : one_day()) r.on_frame(f);
    r.stop();
    return r.total_bytes();
}

int main() {
    Recorder mp4(Container::Mp4, kFramesPerSegment);
    CHECK_EQ(run_day(mp4), kMp4Bytes);
    CHECK(mp4.files() == kMp4Files);

    Recorder mkv(Container::Mkv, kFramesPerSegment);
    CHECK_EQ(run_day(mkv), kMkvBytes);
    CHECK_EQ(mkv.files().back(), std::string("seg-0002.mkv"));

    Recorder raw(Container::RawH264, kFramesPerSegment);
    CHECK_EQ(run_day(raw), kRawBytes);
    CHECK_EQ(raw.files().size(), 3);

    std::cout << "06-factory-method/naive: mp4=" << kMp4Bytes << " mkv=" << kMkvBytes
              << " raw=" << kRawBytes << " segments=3\n";
}
