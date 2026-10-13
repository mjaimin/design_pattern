// 22 Flyweight — the naive version.
//
// Every BoundingBox carries a full copy of its class metadata: label, display
// name, colour, priority and a ~1 KiB icon. 10 000 boxes of four classes means
// 10 000 icon copies for 4 distinct icons.
//
// Where it hurts:
//   - Memory scales with boxes × metadata size, not boxes + classes. A busy
//     scene (crowd, car park) multiplies it; a 24 h track history stores it.
//   - Every box construction allocates and copies strings → allocator churn on
//     the per-frame hot path.
//   - Metadata can drift: nothing stops one "person" box having a different
//     colour from another, because each box owns its own copy.
//   - Changing a class colour in the UI means touching every live box.

#include <cstdint>
#include <string>
#include <vector>

#include "alloc_counter.hpp"
#include "check.hpp"
#include "scene.hpp"

struct BoundingBox {
    // Varies per detection:
    int x, y, w, h;
    float confidence;
    int track_id;
    // Same for every box of a class — but copied into each one:
    int class_index;
    std::string label;
    std::string display_name;
    std::string icon_svg;
    std::uint32_t rgba;
    int priority;
};

std::vector<BoundingBox> build_boxes(const std::vector<RawDetection>& raw) {
    std::vector<BoundingBox> boxes;
    boxes.reserve(raw.size());
    for (const auto& d : raw) {
        const auto& spec = kClassSpecs[static_cast<std::size_t>(d.class_index)];
        boxes.push_back({d.x, d.y, d.w, d.h, d.confidence, d.track_id, d.class_index,
                         std::string(spec.label), std::string(spec.display_name),
                         make_icon_svg(spec.label), spec.rgba, spec.priority});
    }
    return boxes;
}

RenderStats render(const std::vector<BoundingBox>& boxes) {
    RenderStats s;
    for (const auto& b : boxes)
        fold(s, b.class_index, b.rgba, b.x, b.y, b.label.size(), b.icon_svg.size());
    return s;
}

int main() {
    const auto raw = detector_output();

    const std::size_t before = g_heap_bytes;
    const auto boxes = build_boxes(raw);
    const std::size_t used = g_heap_bytes - before;

    const auto stats = render(boxes);
    CHECK_EQ(stats.boxes, kTotalBoxes);
    CHECK_EQ(stats.per_class[0], kExpectedPerClass[0]);
    CHECK_EQ(stats.per_class[1], kExpectedPerClass[1]);
    CHECK_EQ(stats.per_class[2], kExpectedPerClass[2]);
    CHECK_EQ(stats.per_class[3], kExpectedPerClass[3]);
    CHECK_EQ(stats.checksum, kExpectedChecksum);

    // At least one icon allocation per box: memory ∝ number of boxes.
    CHECK(used >= static_cast<std::size_t>(kTotalBoxes) * 1000);

    std::cout << "22-flyweight/naive: boxes=" << stats.boxes << " sizeof(BoundingBox)="
              << sizeof(BoundingBox) << " heap=" << used / 1024 << " KiB\n";
}
