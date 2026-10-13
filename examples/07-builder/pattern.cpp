// 07 Builder — classic GoF form (Builder interface + Director + 2 concrete builders).
//
// Force: a pipeline has many optional stages with ordering and compatibility
//        rules; it must be assembled step by step, invalid combinations must be
//        rejected *during* assembly, and the same recipe ("detect and record")
//        must be reusable to produce different results.
//
// Roles
//   Builder          : PipelineBuilder      — the construction steps, nothing else
//   Concrete builders: GraphBuilder         — produces a validated Pipeline
//                      LoadEstimateBuilder  — produces a CPU/GPU estimate from the same steps
//   Director         : record_only(), detect_and_record(), low_bitrate_restream()
//                      — recipes: which steps, in which order, with which values
//   Product          : Pipeline, Load       — two unrelated result types
//
// The second builder is why GoF Builder exists: the gateway asks "will this
// pipeline fit on this box?" *before* launching it, by running the very same
// recipe against a builder that adds up costs instead of creating stages.

#include <stdexcept>
#include <string>
#include <vector>

#include "check.hpp"
#include "pipeline.hpp"

// ---- Builder ------------------------------------------------------------------
class PipelineBuilder {
public:
    virtual ~PipelineBuilder() = default;
    virtual void source_rtsp(const std::string& url) = 0;
    virtual void decode(bool hw) = 0;
    virtual void infer(const std::string& model, float threshold) = 0;
    virtual void encode(const std::string& codec, int bitrate_kbps) = 0;
    virtual void sink_file(const std::string& path) = 0;
    virtual void sink_restream(const std::string& url) = 0;
};

// ---- Concrete builder 1: a validated pipeline ----------------------------------
struct Pipeline {
    std::vector<Stage> stages;
};

class GraphBuilder final : public PipelineBuilder {
public:
    // Each step checks its own precondition, so a bad recipe fails at the
    // exact step that is wrong — not later when the pipeline starts.
    void source_rtsp(const std::string& url) override {
        require(!source_, "source already set");
        source_ = true;
        add("rtsp", url);
    }
    void decode(bool hw) override {
        require(source_ && !decode_ && !sink_, "decode needs a source, once, before the sink");
        decode_ = true;
        add("decode", decode_args(hw));
    }
    void infer(const std::string& model, float threshold) override {
        require(decode_ && !encode_ && !sink_, "infer needs decoded frames");
        add("infer", infer_args(model, threshold));
    }
    void encode(const std::string& codec, int bitrate_kbps) override {
        require(decode_ && !encode_ && !sink_, "encode needs decoded frames, once");
        encode_ = true;
        add("encode", encode_args(codec, bitrate_kbps));
    }
    void sink_file(const std::string& path) override { sink("file", path); }
    void sink_restream(const std::string& url) override { sink("restream", url); }

    // Completeness is checked once, at the end.
    Pipeline result() {
        require(source_ && sink_, "pipeline needs a source and a sink");
        return Pipeline{std::move(stages_)};
    }

private:
    void sink(const std::string& kind, const std::string& target) {
        require(source_ && !sink_ && decode_ == encode_, "one sink, after re-encoding decoded frames");
        sink_ = true;
        add(kind, target);
    }
    void add(std::string kind, std::string args) { stages_.push_back({std::move(kind), std::move(args)}); }
    static void require(bool ok, const char* why) {
        if (!ok) throw std::invalid_argument(why);
    }

    bool source_ = false, decode_ = false, encode_ = false, sink_ = false;
    std::vector<Stage> stages_;
};

// ---- Concrete builder 2: same steps, different product -------------------------
struct Load {
    int cpu_pct = 0;
    int gpu_pct = 0;
};

class LoadEstimateBuilder final : public PipelineBuilder {
public:
    // Illustrative per-stage costs for one 1080p stream.
    void source_rtsp(const std::string&) override { load_.cpu_pct += 1; }
    void decode(bool hw) override {
        if (hw) load_.gpu_pct += 5;
        else load_.cpu_pct += 20;
    }
    void infer(const std::string&, float) override { load_.gpu_pct += 30; }
    void encode(const std::string&, int bitrate_kbps) override { load_.cpu_pct += bitrate_kbps / 100; }
    void sink_file(const std::string&) override { load_.cpu_pct += 1; }
    void sink_restream(const std::string&) override { load_.cpu_pct += 1; }

    Load result() const { return load_; }

private:
    Load load_;
};

// ---- Director: recipes, written once against the Builder interface ------------
struct CameraConfig {
    std::string id;
    std::string url;
};

void record_only(PipelineBuilder& b, const CameraConfig& cam) {
    b.source_rtsp(cam.url);
    b.sink_file("/rec/" + cam.id);
}

void detect_and_record(PipelineBuilder& b, const CameraConfig& cam) {
    b.source_rtsp(cam.url);
    b.decode(/*hw=*/true);
    b.infer("person", 0.5f);
    b.encode("h264", 2000);
    b.sink_file("/rec/" + cam.id);
}

void low_bitrate_restream(PipelineBuilder& b, const CameraConfig& cam) {
    b.source_rtsp(cam.url);
    b.decode(/*hw=*/false);
    b.encode("h264", 500);
    b.sink_restream("rtsp://0.0.0.0:8554/" + cam.id);
}

template <class Recipe>
Pipeline build_graph(Recipe recipe, const CameraConfig& cam) {
    GraphBuilder b;
    recipe(b, cam);
    return b.result();
}

template <class Recipe>
Load estimate(Recipe recipe, const CameraConfig& cam) {
    LoadEstimateBuilder b;
    recipe(b, cam);
    return b.result();
}

int main() {
    const CameraConfig cam1{"cam-1", "rtsp://10.0.0.11/stream1"};
    const CameraConfig cam2{"cam-2", "rtsp://10.0.0.12/stream1"};
    const CameraConfig cam3{"cam-3", "rtsp://10.0.0.13/stream1"};

    // Same descriptions as naive.cpp.
    const auto p1 = build_graph(record_only, cam1);
    const auto p2 = build_graph(detect_and_record, cam2);
    const auto p3 = build_graph(low_bitrate_restream, cam3);
    CHECK_EQ(describe(p1.stages), kRecordOnly);
    CHECK_EQ(describe(p2.stages), kDetectAndRecord);
    CHECK_EQ(describe(p3.stages), kLowBitrateRestream);
    CHECK_EQ(p1.stages.size() + p2.stages.size() + p3.stages.size(), 2 + 5 + 4);

    // Same recipes, second builder: capacity check before launching anything.
    const Load l2 = estimate(detect_and_record, cam2);
    CHECK_EQ(l2.cpu_pct, 1 + 20 + 1);  // source + encode 2000k + sink
    CHECK_EQ(l2.gpu_pct, 5 + 30);      // hw decode + inference
    CHECK_EQ(estimate(low_bitrate_restream, cam3).cpu_pct, 1 + 20 + 5 + 1);
    CHECK_EQ(estimate(record_only, cam1).gpu_pct, 0);

    // Inference on compressed data: rejected at the infer() step itself.
    int rejected = 0;
    try {
        GraphBuilder b;
        b.source_rtsp("rtsp://10.0.0.14/stream1");
        b.infer("person", 0.5f);
    } catch (const std::invalid_argument&) {
        ++rejected;
    }
    CHECK_EQ(rejected, 1);

    std::cout << "07-builder/pattern: stages=11 rejected=" << rejected
              << " detect_load=cpu" << l2.cpu_pct << "%/gpu" << l2.gpu_pct << "%\n";
}
