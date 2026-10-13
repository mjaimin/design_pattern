// 07 Builder — two modern C++ alternatives.
//
// A. Designated initializers + one validating function
//    C++20 aggregates with named fields remove the *readability* problem of the
//    telescoping constructor: every argument is labelled, omitted fields take
//    their defaults, and std::optional says "stage absent" without magic "".
//    Validation still runs at one point (make_pipeline), at runtime.
//    Wins when: the object is plain data and the rules are few.
//
// B. Type-state fluent builder
//    The builder's *type* records which steps have happened:
//    Builder<HasSource, HasDecode, HasEncode, HasSink>. Each step is only
//    callable in a state where it is legal, and build() only exists once a
//    source and a sink are present. Ordering mistakes become compile errors.
//    Wins when: misuse is costly and the rules are about order/presence —
//    exactly what can be expressed in the type.

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "check.hpp"
#include "pipeline.hpp"

struct Pipeline {
    std::vector<Stage> stages;
};

// ---------------------------------------------------------------------------
// A. Designated initializers
// ---------------------------------------------------------------------------
namespace spec_form {

struct InferSpec {
    std::string model;
    float threshold = 0.5f;
};
struct EncodeSpec {
    std::string codec = "h264";
    int bitrate_kbps = 2000;
};
struct FileSink {
    std::string path;
};
struct RestreamSink {
    std::string url;
};

// Optional fields get an explicit default (`= std::nullopt`): it documents
// "absent unless named", and keeps -Wmissing-field-initializers quiet when a
// designated initializer leaves them out.
struct PipelineSpec {
    std::string source;
    std::optional<bool> decode_hw = std::nullopt;  // nullopt = no decode stage
    std::optional<InferSpec> infer = std::nullopt;
    std::optional<EncodeSpec> encode = std::nullopt;
    std::variant<FileSink, RestreamSink> sink;
};

Pipeline make_pipeline(const PipelineSpec& s) {
    if (s.source.empty()) throw std::invalid_argument("pipeline needs a source");
    if (s.infer && !s.decode_hw) throw std::invalid_argument("infer needs decoded frames");
    if (s.decode_hw.has_value() != s.encode.has_value())
        throw std::invalid_argument("decoded frames must be re-encoded");

    Pipeline p;
    p.stages.push_back({"rtsp", s.source});
    if (s.decode_hw) p.stages.push_back({"decode", decode_args(*s.decode_hw)});
    if (s.infer) p.stages.push_back({"infer", infer_args(s.infer->model, s.infer->threshold)});
    if (s.encode) p.stages.push_back({"encode", encode_args(s.encode->codec, s.encode->bitrate_kbps)});
    if (const auto* f = std::get_if<FileSink>(&s.sink)) p.stages.push_back({"file", f->path});
    else p.stages.push_back({"restream", std::get<RestreamSink>(s.sink).url});
    return p;
}

}  // namespace spec_form

// ---------------------------------------------------------------------------
// B. Type-state builder: illegal sequences do not compile
// ---------------------------------------------------------------------------
namespace typestate_form {

template <bool Src, bool Dec, bool Enc, bool Sink>
class Builder {
public:
    Builder() requires(!Src && !Dec && !Enc && !Sink) = default;

    auto from_rtsp(std::string url) && requires(!Src) {
        return next<true, Dec, Enc, Sink>({"rtsp", std::move(url)});
    }
    auto decode(bool hw) && requires(Src && !Dec && !Sink) {
        return next<Src, true, Enc, Sink>({"decode", decode_args(hw)});
    }
    auto infer(const std::string& model, float threshold) && requires(Dec && !Enc && !Sink) {
        return next<Src, Dec, Enc, Sink>({"infer", infer_args(model, threshold)});
    }
    auto encode(const std::string& codec, int kbps) && requires(Dec && !Enc && !Sink) {
        return next<Src, Dec, true, Sink>({"encode", encode_args(codec, kbps)});
    }
    auto to_file(std::string path) && requires(Src && Dec == Enc && !Sink) {
        return next<Src, Dec, Enc, true>({"file", std::move(path)});
    }
    auto to_restream(std::string url) && requires(Src && Dec == Enc && !Sink) {
        return next<Src, Dec, Enc, true>({"restream", std::move(url)});
    }
    Pipeline build() && requires(Src && Sink) { return Pipeline{std::move(stages_)}; }

private:
    template <bool, bool, bool, bool>
    friend class Builder;

    explicit Builder(std::vector<Stage> stages) : stages_(std::move(stages)) {}

    template <bool S, bool D, bool E, bool K>
    Builder<S, D, E, K> next(Stage s) {
        stages_.push_back(std::move(s));
        return Builder<S, D, E, K>(std::move(stages_));
    }

    std::vector<Stage> stages_;
};

using PipelineBuilder = Builder<false, false, false, false>;

// Compile-time proof that misuse is rejected.
template <class B>
concept CanBuild = requires(B b) { std::move(b).build(); };
template <class B>
concept CanInfer = requires(B b) { std::move(b).infer("person", 0.5f); };

using AfterSource = decltype(PipelineBuilder{}.from_rtsp(""));
static_assert(!CanBuild<AfterSource>);  // no sink yet
static_assert(!CanInfer<AfterSource>);  // infer on compressed data
static_assert(CanInfer<decltype(PipelineBuilder{}.from_rtsp("").decode(true))>);

}  // namespace typestate_form

int main() {
    using namespace spec_form;
    const auto a1 = make_pipeline({.source = "rtsp://10.0.0.11/stream1", .sink = FileSink{"/rec/cam-1"}});
    const auto a2 = make_pipeline({.source = "rtsp://10.0.0.12/stream1",
                                   .decode_hw = true,
                                   .infer = InferSpec{"person", 0.5f},
                                   .encode = EncodeSpec{"h264", 2000},
                                   .sink = FileSink{"/rec/cam-2"}});
    const auto a3 = make_pipeline({.source = "rtsp://10.0.0.13/stream1",
                                   .decode_hw = false,
                                   .encode = EncodeSpec{"h264", 500},
                                   .sink = RestreamSink{"rtsp://0.0.0.0:8554/cam-3"}});
    CHECK_EQ(describe(a1.stages), kRecordOnly);
    CHECK_EQ(describe(a2.stages), kDetectAndRecord);
    CHECK_EQ(describe(a3.stages), kLowBitrateRestream);

    int rejected = 0;
    try {
        make_pipeline({.source = "rtsp://10.0.0.14/stream1",
                       .infer = InferSpec{"person", 0.5f},
                       .sink = FileSink{"/rec/cam-4"}});
    } catch (const std::invalid_argument&) {
        ++rejected;
    }
    CHECK_EQ(rejected, 1);

    using typestate_form::PipelineBuilder;
    const auto b2 = PipelineBuilder{}
                        .from_rtsp("rtsp://10.0.0.12/stream1")
                        .decode(true)
                        .infer("person", 0.5f)
                        .encode("h264", 2000)
                        .to_file("/rec/cam-2")
                        .build();
    CHECK_EQ(describe(b2.stages), kDetectAndRecord);
    const auto b1 = PipelineBuilder{}.from_rtsp("rtsp://10.0.0.11/stream1").to_file("/rec/cam-1").build();
    CHECK_EQ(describe(b1.stages), kRecordOnly);

    std::cout << "07-builder/modern: designated-init spec + type-state builder match pattern.cpp\n";
}
