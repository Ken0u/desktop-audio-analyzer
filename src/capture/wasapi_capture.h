#pragma once
#include "dsp/audio_block.h"
#include "dsp/spsc_queue.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace daa {
using AudioQueue = SpscQueue<AudioBlock, 128>;
struct CaptureStatus {
    std::string state = "Starting";
    std::wstring device;
    std::string error;
    std::uint32_t sampleRate = 0;
};

class WasapiCapture {
public:
    explicit WasapiCapture(AudioQueue& queue);
    ~WasapiCapture();
    void start();
    void stop();
    CaptureStatus status() const;
    std::uint64_t overruns() const { return overruns_.load(); }
    std::uint64_t capturedFrames() const { return frames_.load(); }
private:
    void run();
    void stream();
    void setStatus(CaptureStatus status);
    AudioQueue& queue_;
    std::thread thread_;
    std::atomic<bool> stopping_{false};
    std::atomic<std::uint64_t> overruns_{0}, frames_{0};
    mutable std::mutex statusMutex_;
    CaptureStatus status_;
    void* stopEvent_ = nullptr;
    void* restartEvent_ = nullptr;
    std::uint64_t generation_ = 0;
};
} // namespace daa
