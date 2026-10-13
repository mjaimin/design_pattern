// 00 Foundations — program to an interface, and depend on abstractions.
//
// A ClipExporter decides *what* to store and *where in the tree* it goes.
// Where the bytes physically land (local disk today, object storage tomorrow,
// RAM in a test) is a different decision, owned by someone else.
//
// Direct version (what we avoid):
//     class ClipExporter { void export_clip(...) { std::ofstream f(path) ... } };
// The exporter is welded to the filesystem: to test it you need a disk, and to
// add S3 you edit the exporter.
//
// Inverted version (below): ClipExporter depends on ClipStore (an interface it
// owns). Both the real store and the test store depend on that interface too.
// The arrow of source-code dependency now points *at* the policy, not away
// from it — that is the "inversion" in Dependency Inversion.
//
// Note there are two real implementations here, not one plus a hypothetical:
// an interface with a single implementation is usually just indirection.

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "check.hpp"
#include "domain.hpp"

namespace fs = std::filesystem;
using Bytes = std::vector<char>;

// The abstraction — small, and shaped by what the *exporter* needs.
class ClipStore {
public:
    virtual ~ClipStore() = default;
    virtual void put(const std::string& key, const Bytes& data) = 0;
};

// High-level policy: knows the clip naming scheme, nothing about storage.
class ClipExporter {
public:
    explicit ClipExporter(ClipStore& store) : store_(store) {}  // composition, injected

    std::string export_clip(const gw::CameraId& cam, const std::string& date,
                            gw::TimestampMs start, const Bytes& mp4) {
        const auto minute = (start / gw::at(0, 1)) % 60;
        const std::string key =
            std::format("{}/{}/{:02}{:02}.mp4", cam, date, gw::hour_of_day(start), minute);
        store_.put(key, mp4);
        return key;
    }

private:
    ClipStore& store_;
};

// Low-level detail #1: real files under a root directory.
class LocalDiskStore final : public ClipStore {
public:
    explicit LocalDiskStore(fs::path root) : root_(std::move(root)) {}
    void put(const std::string& key, const Bytes& data) override {
        const fs::path p = root_ / key;
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary).write(data.data(), static_cast<std::streamsize>(data.size()));
    }

private:
    fs::path root_;
};

// Low-level detail #2: RAM, for tests — no disk, instant, inspectable.
class InMemoryStore final : public ClipStore {
public:
    void put(const std::string& key, const Bytes& data) override { blobs[key] = data; }
    std::map<std::string, Bytes> blobs;
};

int main() {
    const Bytes clip = {'f', 't', 'y', 'p'};

    // Unit-test style: the exporter's naming rule, checked without touching disk.
    InMemoryStore mem;
    ClipExporter to_mem(mem);
    CHECK_EQ(to_mem.export_clip("cam-1", "2026-10-13", gw::at(8, 5), clip),
             std::string("cam-1/2026-10-13/0805.mp4"));
    CHECK(mem.blobs.at("cam-1/2026-10-13/0805.mp4") == clip);

    // Same exporter, unchanged, against real files.
    const fs::path root = fs::temp_directory_path() / "design_pattern_00_di";
    fs::remove_all(root);
    LocalDiskStore disk(root);
    ClipExporter to_disk(disk);
    const auto key = to_disk.export_clip("cam-2", "2026-10-13", gw::at(23, 59), clip);
    std::ifstream in(root / key, std::ios::binary);
    const Bytes read_back{std::istreambuf_iterator<char>(in), {}};
    CHECK(read_back == clip);
    fs::remove_all(root);

    std::cout << "00-foundations/dependency_inversion: exporter works on both stores\n";
}
