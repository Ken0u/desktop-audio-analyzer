#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <vector>

namespace daa {
class WavReader {
public:
    explicit WavReader(const std::filesystem::path& path);
    std::uint32_t sampleRate() const { return rate_; }
    std::uint16_t channels() const { return channels_; }
    std::uint64_t frames() const { return frames_; }
    std::uint64_t framesRead() const { return framesRead_; }
    // Always produces interleaved stereo; mono is duplicated, never amplified.
    std::size_t read(std::span<float> stereo);
private:
    std::ifstream input_;
    std::uint32_t rate_ = 0;
    std::uint16_t channels_ = 0, bits_ = 0, alignment_ = 0;
    bool float_ = false;
    std::uint64_t frames_ = 0, framesRead_ = 0;
    std::vector<unsigned char> buffer_;
};

struct ReferenceResult {
    std::uint32_t sampleRate = 0;
    std::uint64_t frames = 0, windows = 0, analyzedFrames = 0;
    std::vector<float> spectrum;
    float samplePeak = 0;
    double rms = 0;
};

ReferenceResult analyzeReference(const std::filesystem::path& path, const std::atomic<bool>& cancel,
                                 const std::function<void(int)>& progress = {});
} // namespace daa
