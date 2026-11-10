#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace daa {
constexpr std::size_t blockFrames = 1024;
struct AudioBlock {
    std::array<float, blockFrames * 2> samples{};
    std::uint32_t frames = 0;
    std::uint32_t sampleRate = 48000;
    std::uint64_t generation = 0;
    std::uint64_t samplePosition = 0;
    std::uint64_t timestamp100ns = 0;
    bool discontinuity = false;
    bool timestampValid = false;
};
} // namespace daa
