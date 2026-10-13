// 22 Flyweight — data-oriented form: a handle instead of a pointer.
//
// When the set of flyweights is known up front (the model's class list), a
// lookup factory is unnecessary: build the table once and refer to entries by
// a small integer handle. Compared with pattern.cpp:
//   - the per-box reference shrinks from 8 bytes (pointer) to 1 byte (index),
//     and the other fields can shrink to fit the domain (pixels fit int16)
//   - no hash lookup per box; no lifetime question (the table is a static)
//   - the table is contiguous and cache-friendly; boxes become trivially
//     copyable — memcpy-able into a ring buffer or a shared-memory segment
// Cost: the set of classes must be closed or at least indexable; adding a class
// at runtime means growing the table and handing out a new index.
//
// This is the same idea as string interning, enum-indexed lookup tables, and
// an ECS (entity-component-system) storing a component id instead of a copy.

#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "alloc_counter.hpp"
#include "check.hpp"
#include "scene.hpp"

struct DetectionClass {
    std::string_view label;
    std::string_view display_name;
    std::string icon_svg;
    std::uint32_t rgba;
    int priority;
};

using ClassId = std::uint8_t;  // up to 256 classes — plenty for one model

// Built once on first use; never mutated afterwards.
const std::array<DetectionClass, kClassSpecs.size()>& class_table() {
    static const auto table = [] {
        std::array<DetectionClass, kClassSpecs.size()> t{};
        for (std::size_t i = 0; i < t.size(); ++i) {
            const auto& s = kClassSpecs[i];
            t[i] = {s.label, s.display_name, make_icon_svg(s.label), s.rgba, s.priority};
        }
        return t;
    }();
    return table;
}

struct BoundingBox {
    std::int16_t x, y, w, h;  // 1080p fits in 16 bits
    float confidence;
    std::int32_t track_id;
    ClassId cls;              // 1-byte handle into class_table()
};
static_assert(std::is_trivially_copyable_v<BoundingBox>);

std::vector<BoundingBox> build_boxes(const std::vector<RawDetection>& raw) {
    std::vector<BoundingBox> boxes;
    boxes.reserve(raw.size());
    for (const auto& d : raw)
        boxes.push_back({static_cast<std::int16_t>(d.x), static_cast<std::int16_t>(d.y),
                         static_cast<std::int16_t>(d.w), static_cast<std::int16_t>(d.h),
                         d.confidence, d.track_id, static_cast<ClassId>(d.class_index)});
    return boxes;
}

RenderStats render(const std::vector<BoundingBox>& boxes) {
    const auto& table = class_table();
    RenderStats s;
    for (const auto& b : boxes) {
        const auto& c = table[b.cls];
        fold(s, b.cls, c.rgba, b.x, b.y, c.label.size(), c.icon_svg.size());
    }
    return s;
}

int main() {
    const auto raw = detector_output();

    const std::size_t before = g_heap_bytes;
    (void)class_table();  // count the 4 icons too
    const auto boxes = build_boxes(raw);
    const std::size_t used = g_heap_bytes - before;

    const auto stats = render(boxes);
    CHECK_EQ(stats.boxes, kTotalBoxes);
    CHECK_EQ(stats.per_class[0], kExpectedPerClass[0]);
    CHECK_EQ(stats.per_class[1], kExpectedPerClass[1]);
    CHECK_EQ(stats.per_class[2], kExpectedPerClass[2]);
    CHECK_EQ(stats.per_class[3], kExpectedPerClass[3]);
    CHECK_EQ(stats.checksum, kExpectedChecksum);

    CHECK(sizeof(BoundingBox) <= 20);
    CHECK(used <= static_cast<std::size_t>(kTotalBoxes) * sizeof(BoundingBox) + 8 * 1024);

    std::cout << "22-flyweight/modern: boxes=" << stats.boxes << " sizeof(BoundingBox)="
              << sizeof(BoundingBox) << " heap=" << used / 1024 << " KiB\n";
}
