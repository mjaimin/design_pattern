#pragma once
// Chapter 06 products: one SegmentWriter per container format.
// Shared by naive.cpp, pattern.cpp and modern.cpp — the three files differ only
// in *who decides which writer to create*, which is the whole point of the chapter.
//
// Byte counts are simulated but fixed, so totals are checkable:
//   MP4 : 32 B header (ftyp + moov placeholder) + 100 B/frame + 16 B index trailer
//   MKV : 24 B EBML header + (100 + 4 block overhead) B/frame, no trailer
//   raw : 100 B/frame, no container at all

#include <cstddef>
#include <format>
#include <string>
#include <vector>

#include "domain.hpp"

inline constexpr std::size_t kFrameBytes = 100;

// ---- Product ----------------------------------------------------------------
class SegmentWriter {
public:
    virtual ~SegmentWriter() = default;
    virtual std::string file_name() const = 0;
    virtual void write(const gw::Frame& f) = 0;
    virtual void finish() = 0;  // write trailer / index, close the file
    virtual std::size_t bytes() const = 0;
};

// ---- Concrete products --------------------------------------------------------
class Mp4Writer final : public SegmentWriter {
public:
    explicit Mp4Writer(int index) : index_(index) {}
    std::string file_name() const override { return std::format("seg-{:04}.mp4", index_); }
    void write(const gw::Frame&) override { bytes_ += kFrameBytes; }
    void finish() override { bytes_ += 16; }
    std::size_t bytes() const override { return bytes_; }

private:
    int index_;
    std::size_t bytes_ = 32;
};

class MkvWriter final : public SegmentWriter {
public:
    explicit MkvWriter(int index) : index_(index) {}
    std::string file_name() const override { return std::format("seg-{:04}.mkv", index_); }
    void write(const gw::Frame&) override { bytes_ += kFrameBytes + 4; }
    void finish() override {}
    std::size_t bytes() const override { return bytes_; }

private:
    int index_;
    std::size_t bytes_ = 24;
};

class RawH264Writer final : public SegmentWriter {
public:
    explicit RawH264Writer(int index) : index_(index) {}
    std::string file_name() const override { return std::format("seg-{:04}.h264", index_); }
    void write(const gw::Frame&) override { bytes_ += kFrameBytes; }
    void finish() override {}
    std::size_t bytes() const override { return bytes_; }

private:
    int index_;
    std::size_t bytes_ = 0;
};

// ---- Test input & expected results -------------------------------------------
// One frame per minute for 24 h; 600 frames per segment → 3 segments (600, 600, 240).
inline constexpr int kFramesPerSegment = 600;

inline std::vector<gw::Frame> one_day() {
    std::vector<gw::Frame> frames;
    frames.reserve(24 * 60);
    for (int minute = 0; minute < 24 * 60; ++minute) frames.push_back({"cam-1", gw::at(0, minute), false});
    return frames;
}

inline const std::vector<std::string> kMp4Files = {"seg-0000.mp4", "seg-0001.mp4", "seg-0002.mp4"};
inline constexpr std::size_t kMp4Bytes = 3 * (32 + 16) + 1440 * 100;  // 144'144
inline constexpr std::size_t kMkvBytes = 3 * 24 + 1440 * (100 + 4);   // 149'832
inline constexpr std::size_t kRawBytes = 1440 * 100;                  // 144'000
