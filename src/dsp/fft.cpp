#include "dsp/fft.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace daa {
SpectrumTransform::SpectrumTransform(std::size_t size)
    : size_(size), window_(size), reversed_(size), roots_(size / 2), scratch_(size) {
    if (size < 4 || (size & (size - 1)) != 0) throw std::invalid_argument("FFT size must be a power of two >= 4");
    std::size_t bits = 0;
    for (auto n = size; n > 1; n >>= 1) ++bits;
    for (std::size_t i = 0; i < size; ++i) {
        window_[i] = static_cast<float>(0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / size));
        windowEnergy_ += window_[i] * window_[i];
        std::size_t reversed = 0, value = i;
        for (std::size_t b = 0; b < bits; ++b) { reversed = (reversed << 1) | (value & 1); value >>= 1; }
        reversed_[i] = reversed;
    }
    for (std::size_t i = 0; i < roots_.size(); ++i) {
        const auto angle = -2 * std::numbers::pi * i / size;
        roots_[i] = {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
    }
}

void SpectrumTransform::power(std::span<const float> samples, double rate, std::span<float> result) {
    if (samples.size() != size_ || result.size() != size_ / 2 + 1 || rate <= 0)
        throw std::invalid_argument("Incorrect FFT input/output shape or rate");
    for (std::size_t i = 0; i < size_; ++i) scratch_[reversed_[i]] = {samples[i] * window_[i], 0};
    for (std::size_t length = 2; length <= size_; length *= 2) {
        for (std::size_t base = 0; base < size_; base += length) {
            for (std::size_t j = 0; j < length / 2; ++j) {
                const auto even = scratch_[base + j];
                const auto odd = scratch_[base + j + length / 2] * roots_[j * size_ / length];
                scratch_[base + j] = even + odd;
                scratch_[base + j + length / 2] = even - odd;
            }
        }
    }
    const auto normalization = 1.0 / (rate * windowEnergy_);
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto oneSided = (i == 0 || i == size_ / 2) ? 1.0 : 2.0;
        result[i] = static_cast<float>(std::norm(scratch_[i]) * normalization * oneSided);
    }
}
} // namespace daa
