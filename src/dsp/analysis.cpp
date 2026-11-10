#include "dsp/analysis.h"
#include <algorithm>
#include <cmath>

namespace daa {
float powerDb(float power) { return 10 * std::log10(std::max(power, 1e-16f)); }

std::vector<float> logSpectrum(std::span<const float> power, double rate, std::size_t size,
                              int columns, float minimum, float maximum) {
    std::vector<float> result(static_cast<std::size_t>(std::max(columns, 0)), -160);
    if (power.empty() || columns <= 0 || rate <= 0 || size == 0) return result;
    maximum = std::min(maximum, static_cast<float>(rate / 2));
    if (minimum <= 0 || maximum <= minimum) return result;
    const auto binHz = rate / size;
    for (int x = 0; x < columns; ++x) {
        const double low = minimum * std::pow(maximum / minimum, double(x) / columns);
        const double high = minimum * std::pow(maximum / minimum, double(x + 1) / columns);
        double energy = 0, bandwidth = 0;
        const auto first = std::max(0, static_cast<int>(std::floor(low / binHz - 0.5)));
        const auto last = std::min(static_cast<int>(power.size()) - 1, static_cast<int>(std::ceil(high / binHz + 0.5)));
        for (int bin = first; bin <= last; ++bin) {
            const auto overlap = std::max(0.0, std::min(high, (bin + 0.5) * binHz) - std::max(low, (bin - 0.5) * binHz));
            energy += power[static_cast<std::size_t>(bin)] * overlap;
            bandwidth += overlap;
        }
        result[static_cast<std::size_t>(x)] = powerDb(static_cast<float>(energy / std::max(bandwidth, 1e-20)));
    }
    return result;
}

Analysis::Analysis(double historySeconds)
    : fft_(fftSize), historySeconds_(historySeconds), powerLeft_(fftSize / 2 + 1),
      powerRight_(fftSize / 2 + 1), spectrum_(fftSize / 2 + 1), spectrogram_(192 * 512, -160) {
    reset(48000, 0);
}

void Analysis::reset(std::uint32_t rate, std::uint64_t generation) {
    rate_ = rate;
    generation_ = generation;
    ++segments_;
    frames_ = 0;
    spectralSequence_ = 0;
    envelopes_.assign(static_cast<std::size_t>(historySeconds_ * envelopeRate), {});
    envelopeWrite_ = envelopeCount_ = sampleWrite_ = fftFrames_ = hopFrames_ = rowWrite_ = rowCount_ = 0;
    current_ = {};
    envelopeFrames_ = 0;
    squareLeft_ = squareRight_ = 0;
    peakLeft_ = peakRight_ = rmsLeft_ = rmsRight_ = 0;
    std::fill(left_.begin(), left_.end(), 0);
    std::fill(right_.begin(), right_.end(), 0);
    std::fill(spectrum_.begin(), spectrum_.end(), 0);
    rowCapacity_ = std::max(std::size_t{1}, static_cast<std::size_t>(std::ceil(historySeconds_ * rate_ / (fftSize / 4))));
    spectrogram_.assign(rowCapacity_ * 192, -160);
}

void Analysis::process(const AudioBlock& block) {
    if (block.sampleRate == 0 || block.frames > blockFrames) return;
    if (rate_ != block.sampleRate || generation_ != block.generation || block.discontinuity)
        reset(block.sampleRate, block.generation);
    const auto envelopeLength = std::max(1u, rate_ / envelopeRate);
    // 300 ms exponential RMS integration; this is explicitly not a VU or LUFS meter.
    const double decay = std::exp(-1.0 / (rate_ * 0.3));
    for (std::uint32_t f = 0; f < block.frames; ++f) {
        float l = block.samples[2 * f], r = block.samples[2 * f + 1];
        if (!std::isfinite(l)) l = 0;
        if (!std::isfinite(r)) r = 0;
        if (envelopeFrames_ == 0) current_ = {l, l, r, r};
        current_.minLeft = std::min(current_.minLeft, l); current_.maxLeft = std::max(current_.maxLeft, l);
        current_.minRight = std::min(current_.minRight, r); current_.maxRight = std::max(current_.maxRight, r);
        if (++envelopeFrames_ >= static_cast<int>(envelopeLength)) {
            envelopes_[envelopeWrite_] = current_;
            envelopeWrite_ = (envelopeWrite_ + 1) % envelopes_.size();
            envelopeCount_ = std::min(envelopeCount_ + 1, envelopes_.size());
            envelopeFrames_ = 0;
        }
        peakLeft_ = std::max(std::abs(l), peakLeft_ * 0.9999f);
        peakRight_ = std::max(std::abs(r), peakRight_ * 0.9999f);
        squareLeft_ = decay * squareLeft_ + (1 - decay) * l * l;
        squareRight_ = decay * squareRight_ + (1 - decay) * r * r;
        left_[sampleWrite_] = l; right_[sampleWrite_] = r;
        sampleWrite_ = (sampleWrite_ + 1) % fftSize;
        ++fftFrames_; ++hopFrames_; ++frames_;
        if (fftFrames_ >= fftSize && hopFrames_ >= fftSize / 4) { transform(); hopFrames_ = 0; }
    }
    rmsLeft_ = static_cast<float>(std::sqrt(squareLeft_));
    rmsRight_ = static_cast<float>(std::sqrt(squareRight_));
}

void Analysis::transform() {
    for (std::size_t i = 0; i < fftSize; ++i) {
        orderedLeft_[i] = left_[(sampleWrite_ + i) % fftSize];
        orderedRight_[i] = right_[(sampleWrite_ + i) % fftSize];
    }
    fft_.power(orderedLeft_, rate_, powerLeft_);
    fft_.power(orderedRight_, rate_, powerRight_);
    for (std::size_t i = 0; i < spectrum_.size(); ++i) spectrum_[i] = (powerLeft_[i] + powerRight_[i]) * 0.5f;
    const auto row = logSpectrum(spectrum_, rate_, fftSize, 192);
    std::copy(row.begin(), row.end(), spectrogram_.begin() + static_cast<std::ptrdiff_t>(rowWrite_ * 192));
    rowWrite_ = (rowWrite_ + 1) % rowCapacity_;
    rowCount_ = std::min(rowCount_ + 1, rowCapacity_);
    ++spectralSequence_;
}

AnalysisSnapshot Analysis::snapshot() const {
    AnalysisSnapshot result;
    result.sampleRate = rate_; result.generation = generation_; result.segments = segments_; result.totalFrames = frames_;
    result.spectrogramSequence = spectralSequence_;
    result.peakLeft = peakLeft_; result.peakRight = peakRight_; result.rmsLeft = rmsLeft_; result.rmsRight = rmsRight_;
    result.spectrum = spectrum_;
    result.waveform.reserve(envelopeCount_);
    for (std::size_t i = 0; i < envelopeCount_; ++i)
        result.waveform.push_back(envelopes_[(envelopeWrite_ + envelopes_.size() - envelopeCount_ + i) % envelopes_.size()]);
    result.spectrogramRows = static_cast<int>(rowCount_);
    result.spectrogram.resize(rowCount_ * 192);
    for (std::size_t i = 0; i < rowCount_; ++i) {
        const auto row = (rowWrite_ + rowCapacity_ - rowCount_ + i) % rowCapacity_;
        std::copy_n(spectrogram_.begin() + static_cast<std::ptrdiff_t>(row * 192), 192,
                    result.spectrogram.begin() + static_cast<std::ptrdiff_t>(i * 192));
    }
    return result;
}
} // namespace daa
