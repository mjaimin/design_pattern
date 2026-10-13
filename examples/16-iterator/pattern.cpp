// 16 Iterator — classic form, written as a standard C++ forward iterator.
//
// Force: how segments are laid out (map of days today, maybe a ring buffer
//        or per-camera index tomorrow) varies; callers that walk "every
//        segment, in time order" must not change when it does.
//
// Roles
//   Aggregate         : SegmentStore — owns the data, hands out iterators
//   Iterator          : SegmentStore::const_iterator — knows the layout
//                       (day iterator + position inside that day's vector)
//   Client            : the export / retention / timeline code
//
// In C++ the GoF interface (first / next / is_done / current_item) is spelled
// begin() / ++it / it == end() / *it. Writing it that way means every
// standard algorithm and range-for works on the store for free.

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>

#include "check.hpp"
#include "segments.hpp"

class SegmentStore {
public:
    explicit SegmentStore(DayIndex index) : index_(std::move(index)) {}

    class const_iterator {
    public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = Segment;
        using difference_type = std::ptrdiff_t;
        using pointer = const Segment*;
        using reference = const Segment&;

        const_iterator() = default;

        reference operator*() const { return day_->second[pos_]; }
        pointer operator->() const { return &**this; }

        const_iterator& operator++() {
            ++pos_;
            settle();
            return *this;
        }
        const_iterator operator++(int) {
            auto old = *this;
            ++*this;
            return old;
        }

        bool operator==(const const_iterator& o) const {
            return day_ == o.day_ && (day_ == end_ || pos_ == o.pos_);
        }

    private:
        friend class SegmentStore;
        using DayIt = DayIndex::const_iterator;

        const_iterator(DayIt day, DayIt end) : day_(day), end_(end) { settle(); }

        // Skip past finished and empty days, so *it is always valid.
        void settle() {
            while (day_ != end_ && pos_ >= day_->second.size()) {
                ++day_;
                pos_ = 0;
            }
        }

        DayIt day_{};
        DayIt end_{};
        std::size_t pos_ = 0;
    };

    const_iterator begin() const { return {index_.begin(), index_.end()}; }
    const_iterator end() const { return {index_.end(), index_.end()}; }

private:
    DayIndex index_;  // no longer visible to anyone
};

static_assert(std::forward_iterator<SegmentStore::const_iterator>);

// Clients: one sequence, standard algorithms, no knowledge of days.
int count_all(const SegmentStore& store) {
    return static_cast<int>(std::distance(store.begin(), store.end()));
}

int camera_segments(const SegmentStore& store, const gw::CameraId& cam) {
    return static_cast<int>(
        std::count_if(store.begin(), store.end(), [&](const Segment& s) { return s.camera == cam; }));
}

gw::TimestampMs camera_duration(const SegmentStore& store, const gw::CameraId& cam) {
    gw::TimestampMs total = 0;
    for (const Segment& s : store)  // range-for works because begin()/end() exist
        if (s.camera == cam) total += s.duration;
    return total;
}

int main() {
    // Same numbers as naive.cpp.
    SegmentStore store(sample_index());
    CHECK_EQ(count_all(store), kTotalSegments);
    CHECK_EQ(camera_segments(store, "cam-1"), kCam1Segments);
    CHECK_EQ(camera_duration(store, "cam-1"), kCam1Duration);

    // The empty middle day is skipped: the 6th segment is the first of day 3.
    auto it = std::next(store.begin(), 5);
    CHECK_EQ(it->start, gw::at(22, 0));

    // Two independent iterators over the same store do not interfere.
    auto a = store.begin(), b = store.begin();
    ++a;
    CHECK(a != b);

    std::cout << "16-iterator/pattern: total=" << kTotalSegments << " cam-1=" << kCam1Segments
              << " cam-1-ms=" << kCam1Duration << "\n";
}
