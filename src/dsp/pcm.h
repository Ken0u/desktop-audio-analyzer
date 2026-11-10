#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace daa {
inline float decodePcm(const unsigned char* p, unsigned bits, bool floatingPoint) {
    if (floatingPoint && bits == 32) { float value; std::memcpy(&value, p, 4); return value; }
    if (floatingPoint) throw std::invalid_argument("Only 32-bit floating point audio is supported");
    switch (bits) {
    case 8: return (static_cast<int>(*p) - 128) / 128.0f;
    case 16: { std::int16_t value; std::memcpy(&value, p, 2); return value / 32768.0f; }
    case 24: {
        std::int32_t value = p[0] | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16);
        if (value & 0x800000) value -= 0x1000000;
        return static_cast<float>(value / 8388608.0);
    }
    case 32: { std::int32_t value; std::memcpy(&value, p, 4); return static_cast<float>(value / 2147483648.0); }
    default: throw std::invalid_argument("Unsupported PCM sample width");
    }
}
} // namespace daa
