#pragma once
#include "dsp/audio_block.h"
#include "dsp/fft.h"
#include <array>
#include <vector>

namespace daa {
struct Envelope {
    float minLeft = 0, maxLeft = 0, minRight = 0, maxRight = 0;
};
struct AnalysisSnapshot {
    std::vector<Envelope> waveform;
    std::vector<float> spectrum; // combined stereo power / Hz, before display processing
    std::vector<float> spectrogram; // row-major chronological log-frequency rows, in dB
    int spectrogramColumns = 192;
    int spectrogramRows = 0;
    std::uint32_t sampleRate = 48000;
    std::uint64_t generation = 0;
    std::uint64_t segments = 0;
    std::uint64_t totalFrames = 0;
    std::uint64_t spectrogramSequence = 0;
    float peakLeft = 0, peakRight = 0;
    float rmsLeft = 0, rmsRight = 0;
};

float powerDb(float power);
// Log-frequency display uses energy-preserving bin-band integration / bandwidth.
std::vector<float> logSpectrum(std::span<const float> power, double rate, std::size_t fftSize,
                               int columns, float minimumHz = 20, float maximumHz = 20000);

class Analysis {
public:
    static constexpr std::size_t fftSize = 4096;
    static constexpr int envelopeRate = 200;
    explicit Analysis(double historySeconds = 10);
    void process(const AudioBlock& block);
    void reset(std::uint32_t rate, std::uint64_t generation);
    AnalysisSnapshot snapshot() const;
private:
    void transform();
    SpectrumTransform fft_;
    double historySeconds_;
    std::uint32_t rate_ = 48000;
    std::uint64_t generation_ = 0, segments_ = 0, frames_ = 0, spectralSequence_ = 0;
    std::vector<Envelope> envelopes_;
    std::size_t envelopeWrite_ = 0, envelopeCount_ = 0;
    Envelope current_{};
    int envelopeFrames_ = 0;
    std::array<float, fftSize> left_{}, right_{};
    std::array<float, fftSize> orderedLeft_{}, orderedRight_{};
    std::vector<float> powerLeft_, powerRight_, spectrum_, spectrogram_;
    std::size_t sampleWrite_ = 0, fftFrames_ = 0, hopFrames_ = 0;
    std::size_t rowWrite_ = 0, rowCount_ = 0, rowCapacity_ = 512;
    double squareLeft_ = 0, squareRight_ = 0;
    float peakLeft_ = 0, peakRight_ = 0;
    float rmsLeft_ = 0, rmsRight_ = 0;
};
} // namespace daa
