#pragma once
#include <complex>
#include <span>
#include <vector>

namespace daa {
// Radix-2 FFT with a precomputed Hann window and reusable scratch storage.
// Output is one-sided power spectral density (full-scale squared / Hz).
class SpectrumTransform {
public:
    explicit SpectrumTransform(std::size_t size = 4096);
    std::size_t size() const { return size_; }
    void power(std::span<const float> samples, double sampleRate, std::span<float> result);
private:
    std::size_t size_;
    double windowEnergy_ = 0;
    std::vector<float> window_;
    std::vector<std::size_t> reversed_;
    std::vector<std::complex<float>> roots_;
    std::vector<std::complex<float>> scratch_;
};
} // namespace daa
