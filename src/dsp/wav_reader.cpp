#include "dsp/wav_reader.h"
#include "dsp/analysis.h"
#include "dsp/pcm.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace daa {
namespace {
std::uint16_t u16(const unsigned char* p) { return p[0] | (std::uint16_t(p[1]) << 8); }
std::uint32_t u32(const unsigned char* p) { return u16(p) | (std::uint32_t(u16(p + 2)) << 16); }
void exact(std::ifstream& input, unsigned char* data, std::size_t bytes) {
    if (!input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(bytes)))
        throw std::runtime_error("Truncated WAV file");
}
}

WavReader::WavReader(const std::filesystem::path& path) : input_(path, std::ios::binary) {
    if (!input_) throw std::runtime_error("Cannot open reference file");
    input_.seekg(0, std::ios::end);
    const auto fileSize = static_cast<std::uint64_t>(input_.tellg());
    input_.seekg(0);
    std::array<unsigned char, 12> header{};
    exact(input_, header.data(), header.size());
    if (std::memcmp(header.data(), "RIFF", 4) || std::memcmp(header.data() + 8, "WAVE", 4))
        throw std::runtime_error("This prototype accepts RIFF WAV files only");
    const auto end = std::uint64_t(u32(header.data() + 4)) + 8;
    if (end > fileSize || end < 12) throw std::runtime_error("Invalid RIFF length");
    bool haveFormat = false, haveData = false;
    std::uint64_t dataStart = 0, dataBytes = 0, position = 12;
    while (position + 8 <= end) {
        input_.seekg(static_cast<std::streamoff>(position));
        std::array<unsigned char, 8> chunk{};
        exact(input_, chunk.data(), chunk.size());
        const auto length = u32(chunk.data() + 4);
        if (position + 8 + length > end) throw std::runtime_error("WAV chunk extends beyond RIFF data");
        if (!std::memcmp(chunk.data(), "fmt ", 4) && !haveFormat) {
            if (length < 16) throw std::runtime_error("Invalid WAV format chunk");
            std::array<unsigned char, 40> format{};
            exact(input_, format.data(), std::min<std::size_t>(format.size(), length));
            auto tag = u16(format.data());
            channels_ = u16(format.data() + 2); rate_ = u32(format.data() + 4);
            alignment_ = u16(format.data() + 12); bits_ = u16(format.data() + 14);
            if (tag == 0xfffe) {
                const unsigned char guidTail[]{0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};
                if (length < 40 || u16(format.data() + 16) < 22 ||
                    std::memcmp(format.data() + 26, guidTail, sizeof(guidTail)))
                    throw std::runtime_error("Unsupported extensible WAV subformat");
                tag = u16(format.data() + 24);
            }
            float_ = tag == 3;
            if (tag != 1 && tag != 3) throw std::runtime_error("Compressed WAV is not supported yet");
            if (channels_ < 1 || channels_ > 2) throw std::runtime_error("Choose a mono or stereo reference");
            if (rate_ < 8000 || rate_ > 192000) throw std::runtime_error("WAV sample rate must be between 8 and 192 kHz");
            if ((bits_ != 8 && bits_ != 16 && bits_ != 24 && bits_ != 32) || (float_ && bits_ != 32) ||
                alignment_ != channels_ * bits_ / 8) throw std::runtime_error("Unsupported WAV sample format");
            haveFormat = true;
        } else if (!std::memcmp(chunk.data(), "data", 4) && !haveData) {
            dataStart = position + 8; dataBytes = length; haveData = true;
        }
        position += 8 + length + (length & 1);
    }
    if (!haveFormat || !haveData || dataBytes == 0 || dataBytes % alignment_)
        throw std::runtime_error("WAV has no complete audio data");
    frames_ = dataBytes / alignment_;
    buffer_.resize(blockFrames * alignment_);
    input_.seekg(static_cast<std::streamoff>(dataStart));
}

std::size_t WavReader::read(std::span<float> stereo) {
    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>({frames_ - framesRead_, stereo.size() / 2, blockFrames}));
    if (count == 0) return 0;
    exact(input_, buffer_.data(), count * alignment_);
    for (std::size_t i = 0; i < count; ++i) {
        const auto* sample = buffer_.data() + i * alignment_;
        stereo[2 * i] = decodePcm(sample, bits_, float_);
        stereo[2 * i + 1] = channels_ == 1 ? stereo[2 * i] : decodePcm(sample + bits_ / 8, bits_, float_);
        if (!std::isfinite(stereo[2 * i]) || !std::isfinite(stereo[2 * i + 1]))
            throw std::runtime_error("Reference contains non-finite floating point samples");
    }
    framesRead_ += count;
    return count;
}

ReferenceResult analyzeReference(const std::filesystem::path& path, const std::atomic<bool>& cancel,
                                 const std::function<void(int)>& progress) {
    WavReader reader(path);
    ReferenceResult result;
    result.sampleRate = reader.sampleRate(); result.frames = reader.frames();
    result.spectrum.assign(Analysis::fftSize / 2 + 1, 0);
    SpectrumTransform fft(Analysis::fftSize);
    std::array<float, Analysis::fftSize> left{}, right{}, orderedLeft{}, orderedRight{};
    std::array<float, blockFrames * 2> block{};
    std::vector<float> pl(result.spectrum.size()), pr(result.spectrum.size());
    std::vector<double> sums(result.spectrum.size());
    std::size_t write = 0, received = 0, hop = 0;
    double square = 0;
    int previousProgress = -1;
    while (const auto count = reader.read(block)) {
        if (cancel.load()) throw std::runtime_error("Reference analysis cancelled");
        for (std::size_t i = 0; i < count; ++i) {
            const auto l = block[2 * i], r = block[2 * i + 1];
            result.samplePeak = std::max({result.samplePeak, std::abs(l), std::abs(r)});
            square += (double(l) * l + double(r) * r) * 0.5;
            left[write] = l; right[write] = r;
            write = (write + 1) % Analysis::fftSize;
            ++received; ++hop;
            if (received >= Analysis::fftSize && hop >= Analysis::fftSize / 4) {
                for (std::size_t j = 0; j < Analysis::fftSize; ++j) {
                    orderedLeft[j] = left[(write + j) % Analysis::fftSize];
                    orderedRight[j] = right[(write + j) % Analysis::fftSize];
                }
                fft.power(orderedLeft, result.sampleRate, pl); fft.power(orderedRight, result.sampleRate, pr);
                for (std::size_t j = 0; j < sums.size(); ++j) sums[j] += (double(pl[j]) + pr[j]) * 0.5;
                ++result.windows; result.analyzedFrames = received; hop = 0;
            }
        }
        const auto percent = static_cast<int>(100 * reader.framesRead() / reader.frames());
        if (progress && percent != previousProgress) { progress(percent); previousProgress = percent; }
    }
    if (result.windows == 0) throw std::runtime_error("Reference needs at least 4096 samples");
    for (std::size_t j = 0; j < sums.size(); ++j) result.spectrum[j] = static_cast<float>(sums[j] / result.windows);
    result.rms = std::sqrt(square / result.frames);
    return result;
}
} // namespace daa
