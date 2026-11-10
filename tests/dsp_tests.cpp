#include "dsp/analysis.h"
#include "dsp/pcm.h"
#include "dsp/spsc_queue.h"
#include "dsp/wav_reader.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <thread>

namespace {
int checks = 0;
void expect(bool value, const char* description) {
    ++checks;
    if (!value) throw std::runtime_error(description);
}
template<typename F> void rejects(F operation, const char* description) {
    bool thrown = false;
    try { operation(); } catch (const std::exception&) { thrown = true; }
    expect(thrown, description);
}
void u16(std::ofstream& out, unsigned value) { const char b[]{char(value), char(value >> 8)}; out.write(b, 2); }
void u32(std::ofstream& out, unsigned value) { u16(out, value); u16(out, value >> 16); }
void writeWav(const std::filesystem::path& path, unsigned channels, unsigned frames) {
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4); u32(out, 36 + frames * channels * 2); out.write("WAVEfmt ", 8);
    u32(out, 16); u16(out, 1); u16(out, channels); u32(out, 48000);
    u32(out, 48000 * channels * 2); u16(out, channels * 2); u16(out, 16);
    out.write("data", 4); u32(out, frames * channels * 2);
    for (unsigned i = 0; i < frames; ++i) {
        const auto sample = static_cast<short>(16000 * std::sin(2 * std::numbers::pi * 1500 * i / 48000));
        for (unsigned c = 0; c < channels; ++c) u16(out, static_cast<unsigned short>(c ? -sample : sample));
    }
}
void testQueue() {
    daa::SpscQueue<int, 4> queue;
    int value = -1;
    expect(!queue.pop(value), "Empty queue must not return data");
    expect(queue.push(1) && queue.push(2) && queue.push(3), "Queue usable capacity");
    expect(!queue.push(4), "Full queue must reject without overwriting");
    expect(queue.pop(value) && value == 1, "Queue preserves order");
    expect(queue.push(4), "Queue wraps");
    for (int expected = 2; expected <= 4; ++expected)
        expect(queue.pop(value) && value == expected, "Wrapped queue order");
    daa::SpscQueue<unsigned, 128> concurrent;
    std::atomic<bool> correct{true};
    std::thread consumer([&] {
        for (unsigned i = 0; i < 200000; ++i) {
            unsigned item = 0;
            while (!concurrent.pop(item)) std::this_thread::yield();
            if (item != i) correct = false;
        }
    });
    for (unsigned i = 0; i < 200000; ++i)
        while (!concurrent.push(i)) std::this_thread::yield();
    consumer.join();
    expect(correct, "Concurrent queue must preserve every sample block");
}
void testFft() {
    constexpr std::size_t size = 4096;
    daa::SpectrumTransform fft(size);
    std::vector<float> input(size), power(size / 2 + 1);
    for (std::size_t i = 0; i < size; ++i) input[i] = float(0.5 * std::sin(2 * std::numbers::pi * 128 * i / size));
    fft.power(input, 48000, power);
    const auto peak = std::max_element(power.begin(), power.end()) - power.begin();
    expect(peak == 128, "Bin-centered tone frequency must be exact");
    double energy = 0;
    for (const auto p : power) energy += p * 48000 / size;
    expect(std::abs(energy - 0.125) < 1e-5, "Integrated one-sided PSD must recover mean-square power");
    expect(power[126] < power[128] * 1e-8, "Hann window rejects distant bin leakage");
    std::fill(input.begin(), input.end(), 0.5f);
    fft.power(input, 48000, power);
    energy = 0; for (const auto p : power) energy += p * 48000 / size;
    expect(std::abs(energy - 0.25) < 1e-5, "DC must not be doubled in one-sided PSD");
    rejects([] { daa::SpectrumTransform invalid(1000); }, "Non-power-of-two FFT must be rejected");
}
void testAnalysis() {
    daa::Analysis analysis;
    daa::AudioBlock block;
    block.frames = daa::blockFrames; block.generation = 1;
    for (unsigned n = 0; n < 8; ++n) {
        for (unsigned i = 0; i < block.frames; ++i) {
            const auto sample = float(0.5 * std::sin(2 * std::numbers::pi * 1500 * (n * block.frames + i) / 48000));
            block.samples[2 * i] = sample; block.samples[2 * i + 1] = -sample;
        }
        analysis.process(block);
    }
    auto result = analysis.snapshot();
    expect(result.totalFrames == 8192, "Analysis accounts for every frame");
    expect(result.spectrum[128] > 0.001, "Stereo power must not cancel anti-phase audio");
    expect(result.spectrogramRows == 5, "75 percent overlap must produce expected history cadence");
    const auto segments = result.segments;
    block.discontinuity = true; block.frames = 1;
    block.samples[0] = 0; block.samples[1] = 0;
    analysis.process(block);
    result = analysis.snapshot();
    expect(result.segments == segments + 1 && result.totalFrames == 1, "Gap must reset the analysis segment");
    expect(result.spectrogramRows == 0, "Gap must not join incompatible spectral histories");
    daa::Analysis transient;
    block = {}; block.frames = 480; block.samples[14] = 0.99f; block.samples[15] = -0.9f;
    transient.process(block); result = transient.snapshot();
    expect(result.waveform.size() == 2, "Envelope cadence");
    expect(result.waveform.front().maxLeft == 0.99f && result.waveform.front().minRight == -0.9f,
           "Waveform reduction must retain single-sample transients");
}
void testReference() {
    const auto folder = std::filesystem::temp_directory_path() / "daa-dsp-tests";
    std::filesystem::create_directories(folder);
    const auto valid = folder / "anti-phase.wav";
    writeWav(valid, 2, 48000);
    std::atomic<bool> cancel{false};
    const auto result = daa::analyzeReference(valid, cancel);
    expect(result.sampleRate == 48000 && result.frames == 48000, "WAV format and duration");
    expect(result.samplePeak > 0.48f && result.samplePeak < 0.50f, "PCM16 decode amplitude");
    expect(result.spectrum[128] > 0.001, "Reference stereo power must not cancel anti-phase content");
    expect(result.windows == 43 && result.analyzedFrames == 47104, "Reference coverage must report complete transform windows");
    cancel = true;
    rejects([&] { daa::analyzeReference(valid, cancel); }, "Reference analysis must be cancellable");
    const auto truncated = folder / "truncated.wav";
    { std::ofstream out(truncated, std::ios::binary); out << "RIFF"; }
    rejects([&] { daa::WavReader invalid(truncated); }, "Truncated file must be rejected");
    const auto surround = folder / "surround.wav";
    writeWav(surround, 6, 100);
    rejects([&] { daa::WavReader invalid(surround); }, "Unsupported channel map must not silently downmix");
    const unsigned char negative24[]{0, 0, 0x80};
    expect(daa::decodePcm(negative24, 24, false) == -1.0f, "PCM24 sign extension");
    std::filesystem::remove(valid); std::filesystem::remove(truncated); std::filesystem::remove(surround);
}
}
int main() {
    try {
        testQueue(); testFft(); testAnalysis(); testReference();
        std::cout << "PASS: " << checks << " checks (queue concurrency, FFT calibration, capture gaps, transients, WAV references)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n'; return 1;
    }
}
