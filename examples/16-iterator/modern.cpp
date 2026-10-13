// 16 Iterator — C++20 ranges: compose the traversal instead of writing it.
//
// The hand-written iterator in pattern.cpp exists to flatten "days → segments"
// into one sequence. C++20 has that adaptor already: std::views::join.
// The store exposes a *view* of all segments; callers narrow it further with
// views::filter / views::transform and feed it to std::ranges algorithms.
//
//   all()              = index_ | values | join          (every segment)
//   for_camera("cam-1") = all() | filter(camera == cam-1)
//
// Views are lazy: nothing is copied, nothing is computed until iterated, and
// the store's layout stays private — callers only see "a range of Segment".

#include <algorithm>
#include <ranges>
#include <string>

#include "check.hpp"
#include "segments.hpp"

class SegmentStore {
public:
    explicit SegmentStore(DayIndex index) : index_(std::move(index)) {}

    // Every segment, in time order, empty days naturally contribute nothing.
    auto all() const { return index_ | std::views::values | std::views::join; }

    auto for_camera(gw::CameraId cam) const {
        return all() | std::views::filter([cam = std::move(cam)](const Segment& s) { return s.camera == cam; });
    }

private:
    DayIndex index_;
};

int main() {
    SegmentStore store(sample_index());

    CHECK_EQ(std::ranges::distance(store.all()), kTotalSegments);
    CHECK_EQ(std::ranges::distance(store.for_camera("cam-1")), kCam1Segments);

    auto durations = store.for_camera("cam-1") | std::views::transform(&Segment::duration);
    gw::TimestampMs total = 0;
    for (gw::TimestampMs d : durations) total += d;
    CHECK_EQ(total, kCam1Duration);

    // Laziness: take the first two cam-2 segments; the rest is never visited.
    int seen = 0;
    for (const Segment& s : store.for_camera("cam-2") | std::views::take(2)) {
        CHECK(s.camera == "cam-2");
        ++seen;
    }
    CHECK_EQ(seen, 2);

    std::cout << "16-iterator/modern: views::join + filter give the same counts as pattern.cpp\n";
}
