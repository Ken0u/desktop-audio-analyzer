#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>

namespace daa {
// Single producer / single consumer. A full queue never blocks capture.
template <typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2);
public:
    bool push(const T& item) noexcept {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next = (write + 1) % Capacity;
        if (next == read_.load(std::memory_order_acquire)) return false;
        (*items_)[write] = item;
        write_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& item) noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire)) return false;
        item = (*items_)[read];
        read_.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }
private:
    std::unique_ptr<std::array<T, Capacity>> items_ = std::make_unique<std::array<T, Capacity>>();
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};
} // namespace daa
