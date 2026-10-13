// 16 Iterator — the naive version.
//
// SegmentStore hands out its internal map, and every caller writes the same
// two nested loops: for each day, for each segment.
//
// Where it hurts:
//   1. The storage layout leaks into every caller. Change the store to a
//      flat ring buffer, or to one index file per camera, and every loop in
//      the export job, the retention job and the timeline API must change.
//   2. The traversal is duplicated: the empty-day skip, the ordering, the
//      "which day is this segment in" bookkeeping — copied per caller.
//   3. Callers can't use standard algorithms (std::ranges::count_if, ...)
//      over "all segments", because there is no single sequence to pass.

#include <string>

#include "check.hpp"
#include "segments.hpp"

class SegmentStore {
public:
    explicit SegmentStore(DayIndex index) : index_(std::move(index)) {}
    const DayIndex& days() const { return index_; }  // the leak

private:
    DayIndex index_;
};

// Caller #1: export job counts segments.
int count_all(const SegmentStore& store) {
    int n = 0;
    for (const auto& [day, segments] : store.days())
        for (const auto& s : segments) {
            (void)s;
            ++n;
        }
    return n;
}

// Caller #2: timeline API sums one camera's duration — same loops again.
gw::TimestampMs camera_duration(const SegmentStore& store, const gw::CameraId& cam) {
    gw::TimestampMs total = 0;
    for (const auto& [day, segments] : store.days())
        for (const auto& s : segments)
            if (s.camera == cam) total += s.duration;
    return total;
}

// Caller #3: retention job counts one camera's segments — and again.
int camera_segments(const SegmentStore& store, const gw::CameraId& cam) {
    int n = 0;
    for (const auto& [day, segments] : store.days())
        for (const auto& s : segments)
            if (s.camera == cam) ++n;
    return n;
}

int main() {
    SegmentStore store(sample_index());
    CHECK_EQ(count_all(store), kTotalSegments);
    CHECK_EQ(camera_segments(store, "cam-1"), kCam1Segments);
    CHECK_EQ(camera_duration(store, "cam-1"), kCam1Duration);

    std::cout << "16-iterator/naive: total=" << kTotalSegments << " cam-1=" << kCam1Segments
              << " cam-1-ms=" << kCam1Duration << "\n";
}
