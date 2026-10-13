#pragma once
// Chapter 07 product: a media pipeline as an ordered list of stages, e.g.
//   rtsp[...] ! decode[hw] ! infer[person@0.50] ! encode[h264@2000k] ! file[...]
// Shared by naive.cpp, pattern.cpp and modern.cpp so all three can be compared
// on the exact same description strings.
//
// Validity rules every construction path must enforce:
//   - exactly one source, first; exactly one sink, last
//   - infer needs decoded frames      (infer  ⇒ decode)
//   - decoded frames must be re-encoded before any sink (decode ⇒ encode)
//   - no decode at all is valid: the compressed stream is recorded/relayed as-is

#include <format>
#include <string>
#include <vector>

struct Stage {
    std::string kind;  // rtsp, decode, infer, encode, file, restream
    std::string args;
};

inline std::string describe(const std::vector<Stage>& stages) {
    std::string out;
    for (const auto& s : stages) {
        if (!out.empty()) out += " ! ";
        out += s.args.empty() ? s.kind : s.kind + "[" + s.args + "]";
    }
    return out;
}

// Stage argument formats, shared so every construction path spells them the same.
inline std::string decode_args(bool hw) { return hw ? "hw" : "sw"; }
inline std::string infer_args(const std::string& model, float threshold) {
    return std::format("{}@{:.2f}", model, threshold);
}
inline std::string encode_args(const std::string& codec, int kbps) {
    return std::format("{}@{}k", codec, kbps);
}

// ---- Expected results ---------------------------------------------------------
inline const std::string kRecordOnly =
    "rtsp[rtsp://10.0.0.11/stream1] ! file[/rec/cam-1]";
inline const std::string kDetectAndRecord =
    "rtsp[rtsp://10.0.0.12/stream1] ! decode[hw] ! infer[person@0.50] ! encode[h264@2000k] ! file[/rec/cam-2]";
inline const std::string kLowBitrateRestream =
    "rtsp[rtsp://10.0.0.13/stream1] ! decode[sw] ! encode[h264@500k] ! restream[rtsp://0.0.0.0:8554/cam-3]";
