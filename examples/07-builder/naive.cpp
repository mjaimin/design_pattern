// 07 Builder — the naive version (telescoping constructor).
//
// Every optional stage of the pipeline is a constructor parameter. "Absent"
// is encoded as an empty string or zero. Nine positional parameters, three of
// them strings next to each other, two bools next to each other.
//
// Where it hurts:
//   1. Call sites are unreadable: Pipeline(url, true, false, "", 0.0f, "h264", 500, ...)
//      — which bool is "decode" and which is "hardware"? Swap them and it compiles.
//   2. Invalid combinations construct happily (infer without decode). valid()
//      exists, but nothing forces anyone to call it before starting the pipeline.
//   3. Adding a stage (say, a privacy-mask stage) adds a tenth parameter and
//      touches every call site, including ones that don't want the stage.
// Smells: Long Parameter List, Primitive Obsession (bool/"" as "not present"),
// and validation that is optional instead of structural.

#include <string>
#include <vector>

#include "check.hpp"
#include "pipeline.hpp"

class Pipeline {
public:
    // Empty string / 0 means "stage not present".
    Pipeline(std::string source_url, bool decode, bool hw_decode, std::string model, float threshold,
             std::string codec, int bitrate_kbps, std::string sink_kind, std::string sink_target)
        : has_source_(!source_url.empty()),
          has_decode_(decode),
          has_infer_(!model.empty()),
          has_encode_(!codec.empty()),
          has_sink_(!sink_kind.empty()) {
        if (has_source_) stages_.push_back({"rtsp", source_url});
        if (has_decode_) stages_.push_back({"decode", decode_args(hw_decode)});
        if (has_infer_) stages_.push_back({"infer", infer_args(model, threshold)});
        if (has_encode_) stages_.push_back({"encode", encode_args(codec, bitrate_kbps)});
        if (has_sink_) stages_.push_back({sink_kind, sink_target});
    }

    // The rules exist — as an afterthought the caller may forget.
    bool valid() const {
        return has_source_ && has_sink_ && (!has_infer_ || has_decode_) && (!has_decode_ || has_encode_);
    }

    std::string describe() const { return ::describe(stages_); }
    std::size_t stage_count() const { return stages_.size(); }

private:
    bool has_source_, has_decode_, has_infer_, has_encode_, has_sink_;
    std::vector<Stage> stages_;
};

int main() {
    const Pipeline record_only("rtsp://10.0.0.11/stream1", false, false, "", 0.0f, "", 0, "file", "/rec/cam-1");
    const Pipeline detect("rtsp://10.0.0.12/stream1", true, true, "person", 0.5f, "h264", 2000, "file", "/rec/cam-2");
    const Pipeline restream("rtsp://10.0.0.13/stream1", true, false, "", 0.0f, "h264", 500, "restream",
                            "rtsp://0.0.0.0:8554/cam-3");

    CHECK_EQ(record_only.describe(), kRecordOnly);
    CHECK_EQ(detect.describe(), kDetectAndRecord);
    CHECK_EQ(restream.describe(), kLowBitrateRestream);
    CHECK_EQ(record_only.stage_count() + detect.stage_count() + restream.stage_count(), 2 + 5 + 4);

    // Inference on compressed data: constructs without complaint.
    const Pipeline broken("rtsp://10.0.0.14/stream1", false, false, "person", 0.5f, "", 0, "file", "/rec/cam-4");
    int rejected = 0;
    if (!broken.valid()) ++rejected;  // only because this caller remembered to ask
    CHECK_EQ(rejected, 1);

    std::cout << "07-builder/naive: stages=11 rejected=" << rejected << "\n";
}
