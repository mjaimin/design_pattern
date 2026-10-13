// 22 Flyweight — classic form (shared intrinsic state + flyweight factory).
//
// Force: the number of boxes grows with scene activity and history length;
//        the number of distinct classes stays tiny and fixed by the model.
//        Per-box memory must not scale with per-class data.
//
// Split each box's state in two:
//   intrinsic  — same for every box of a class, immutable  → DetectionClass,
//                stored once, shared by pointer
//   extrinsic  — differs per detection (position, confidence, track id)
//                → stays in BoundingBox
//
// Roles
//   Flyweight         : DetectionClass          — immutable, shareable
//   Flyweight factory : DetectionClassRegistry  — hands out the one instance per key
//   Client            : BoundingBox + render()  — holds extrinsic state + a pointer

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "alloc_counter.hpp"
#include "check.hpp"
#include "scene.hpp"

// ---- Flyweight: intrinsic state, immutable once built ------------------------
struct DetectionClass {
    const int index;
    const std::string label;
    const std::string display_name;
    const std::string icon_svg;
    const std::uint32_t rgba;
    const int priority;
};

// ---- Flyweight factory ---------------------------------------------------------
// Returns the single shared instance for a label, creating it on first use.
// The raw pointers handed out must stay valid for the registry's lifetime.
// unordered_map already never moves its elements, but boxing each flyweight in
// a unique_ptr makes that guarantee independent of the container: switch to a
// std::vector or a flat map later and no outstanding pointer dangles.
class DetectionClassRegistry {
public:
    const DetectionClass* get(int class_index) {
        const auto& spec = kClassSpecs[static_cast<std::size_t>(class_index)];
        auto it = classes_.find(spec.label);
        if (it == classes_.end()) {
            auto cls = std::make_unique<DetectionClass>(DetectionClass{
                class_index, std::string(spec.label), std::string(spec.display_name),
                make_icon_svg(spec.label), spec.rgba, spec.priority});
            it = classes_.emplace(spec.label, std::move(cls)).first;
        }
        return it->second.get();
    }
    std::size_t size() const { return classes_.size(); }

private:
    std::unordered_map<std::string_view, std::unique_ptr<DetectionClass>> classes_;
};

// ---- Client object: extrinsic state + pointer to the flyweight -----------------
struct BoundingBox {
    int x, y, w, h;
    float confidence;
    int track_id;
    const DetectionClass* cls;  // non-owning; the registry outlives every box
};

std::vector<BoundingBox> build_boxes(const std::vector<RawDetection>& raw,
                                     DetectionClassRegistry& registry) {
    std::vector<BoundingBox> boxes;
    boxes.reserve(raw.size());
    for (const auto& d : raw)
        boxes.push_back({d.x, d.y, d.w, d.h, d.confidence, d.track_id, registry.get(d.class_index)});
    return boxes;
}

// The renderer combines the two halves at draw time: extrinsic from the box,
// intrinsic through the pointer. It is the only place they meet.
RenderStats render(const std::vector<BoundingBox>& boxes) {
    RenderStats s;
    for (const auto& b : boxes)
        fold(s, b.cls->index, b.cls->rgba, b.x, b.y, b.cls->label.size(), b.cls->icon_svg.size());
    return s;
}

int main() {
    const auto raw = detector_output();

    const std::size_t before = g_heap_bytes;
    DetectionClassRegistry registry;
    const auto boxes = build_boxes(raw, registry);
    const std::size_t used = g_heap_bytes - before;

    // Same render result as naive.cpp: the refactor did not change behaviour.
    const auto stats = render(boxes);
    CHECK_EQ(stats.boxes, kTotalBoxes);
    CHECK_EQ(stats.per_class[0], kExpectedPerClass[0]);
    CHECK_EQ(stats.per_class[1], kExpectedPerClass[1]);
    CHECK_EQ(stats.per_class[2], kExpectedPerClass[2]);
    CHECK_EQ(stats.per_class[3], kExpectedPerClass[3]);
    CHECK_EQ(stats.checksum, kExpectedChecksum);

    // Four flyweights, however many boxes.
    CHECK_EQ(registry.size(), kClassSpecs.size());
    CHECK(boxes[0].cls == registry.get(raw[0].class_index));  // shared, not copied

    // Memory ≈ boxes × sizeof(BoundingBox) + 4 × metadata. The 16 KiB bound covers
    // 4 icons, the registry's nodes and buckets — and nothing that scales with boxes.
    CHECK(used <= static_cast<std::size_t>(kTotalBoxes) * sizeof(BoundingBox) + 16 * 1024);

    std::cout << "22-flyweight/pattern: boxes=" << stats.boxes << " classes=" << registry.size()
              << " sizeof(BoundingBox)=" << sizeof(BoundingBox) << " heap=" << used / 1024
              << " KiB\n";
}
