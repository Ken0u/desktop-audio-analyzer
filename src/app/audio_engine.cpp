#include "app/audio_engine.h"
#include <QFileInfo>
#include <QJsonObject>
#include <QMetaObject>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

using namespace std::chrono_literals;

AudioEngine::AudioEngine(bool demo, QObject* parent) : QObject(parent), demo_(demo), capture_(queue_) {
    display_ = std::make_shared<daa::AnalysisSnapshot>();
    if (!demo_) capture_.start();
    worker_ = std::thread([this] { analyze(); });
    connect(&timer_, &QTimer::timeout, this, &AudioEngine::tick);
    timer_.setTimerType(Qt::PreciseTimer); timer_.start(16);
}
AudioEngine::~AudioEngine() {
    timer_.stop(); cancelReference_ = true;
    if (referenceWorker_.joinable()) referenceWorker_.join();
    capture_.stop(); stopping_ = true;
    if (worker_.joinable()) worker_.join();
}
void AudioEngine::analyze() {
    daa::Analysis analysis;
    daa::AudioBlock block;
    auto lastPublish = std::chrono::steady_clock::now();
    auto nextDemo = lastPublish;
    std::uint64_t demoFrame = 0;
    while (!stopping_) {
        bool changed = false;
        if (demo_ && std::chrono::steady_clock::now() >= nextDemo) {
            block.frames = daa::blockFrames; block.sampleRate = 48000; block.generation = 1;
            block.samplePosition = demoFrame;
            for (std::size_t i = 0; i < daa::blockFrames; ++i) {
                const double t = double(demoFrame + i) / 48000;
                const double envelope = 0.15 + 0.65 * std::pow(0.5 + 0.5 * std::sin(t * 2.2), 3);
                block.samples[2 * i] = static_cast<float>(envelope * (0.60 * std::sin(t * 2 * std::numbers::pi * 110)
                    + 0.25 * std::sin(t * 2 * std::numbers::pi * 440) + 0.10 * std::sin(t * 2 * std::numbers::pi * 1760)));
                block.samples[2 * i + 1] = static_cast<float>(envelope * (0.60 * std::sin(t * 2 * std::numbers::pi * 110 + 0.2)
                    + 0.25 * std::sin(t * 2 * std::numbers::pi * 550) + 0.10 * std::sin(t * 2 * std::numbers::pi * 2200)));
            }
            demoFrame += block.frames; analysis.process(block); changed = true;
            nextDemo += std::chrono::microseconds(1000000 * daa::blockFrames / 48000);
        }
        while (queue_.pop(block)) { analysis.process(block); changed = true; }
        const auto now = std::chrono::steady_clock::now();
        if (changed && now - lastPublish >= 15ms) {
            auto snapshot = std::make_shared<daa::AnalysisSnapshot>(analysis.snapshot());
            { std::lock_guard lock(snapshotMutex_); latest_ = std::move(snapshot); }
            lastPublish = now;
        }
        std::this_thread::sleep_for(2ms);
    }
}
void AudioEngine::tick() {
    bool changed = false;
    {
        std::lock_guard lock(snapshotMutex_);
        if (latest_ && latest_ != display_) { display_ = latest_; changed = true; }
    }
    const bool previousActive = active_;
    if (++metricTicks_ >= 6 || status_ == "Starting") {
        metricTicks_ = 0;
        if (demo_) { status_ = "Demo signal"; device_ = "Internal test signal - no desktop audio"; active_ = true; }
        else {
            const auto captureStatus = capture_.status();
            status_ = QString::fromStdString(captureStatus.state);
            device_ = QString::fromStdWString(captureStatus.device);
            if (!captureStatus.error.empty()) device_ = QString::fromStdString(captureStatus.error);
            active_ = captureStatus.state == "Capturing";
        }
        const auto previousRate = rate_;
        rate_ = static_cast<int>(display_->sampleRate);
        emit metricsChanged();
        if (rate_ != previousRate) emit referenceChanged();
    }
    if (changed || previousActive != active_) emit frameChanged();
}
QString AudioEngine::peakText() const {
    if (!active_) return "Peak -- dBFS";
    const auto peak = std::max(display_->peakLeft, display_->peakRight);
    if (peak < 1e-7f) return "Peak -inf dBFS";
    return QString("Peak %1 dBFS").arg(20 * std::log10(peak), 0, 'f', 1);
}
void AudioEngine::setEco(bool enabled) { timer_.setInterval(enabled ? 33 : 16); }
QString AudioEngine::referenceInfo() const {
    if (reference_ && reference_->sampleRate != static_cast<unsigned>(rate_))
        return referenceInfo_ + QString(" | Overlay unavailable: source is %1 Hz").arg(rate_);
    return referenceInfo_;
}
void AudioEngine::importReference(const QUrl& url) {
    if (referenceBusy_) return;
    if (!url.isLocalFile()) { referenceInfo_ = "Choose a local WAV file"; emit referenceChanged(); return; }
    if (referenceWorker_.joinable()) referenceWorker_.join();
    const auto path = url.toLocalFile();
    referenceBusy_ = true; cancelReference_ = false; referenceProgress_ = 0; reference_.reset();
    referenceInfo_ = "Analyzing " + QFileInfo(path).fileName(); emit referenceChanged();
    referenceWorker_ = std::thread([this, path] {
        try {
            auto result = std::make_shared<daa::ReferenceResult>(daa::analyzeReference(
                std::filesystem::path(path.toStdWString()), cancelReference_, [this](int value) {
                    QMetaObject::invokeMethod(this, [this, value] { referenceProgress_ = value; emit referenceChanged(); }, Qt::QueuedConnection);
                }));
            QMetaObject::invokeMethod(this, [this, result, path] {
                reference_ = result; referenceBusy_ = false;
                referenceInfo_ = QString("%1 | %2 s | %3 Hz | peak %4 dBFS")
                    .arg(QFileInfo(path).fileName()).arg(double(result->frames) / result->sampleRate, 0, 'f', 1)
                    .arg(result->sampleRate).arg(20 * std::log10(std::max(result->samplePeak, 1e-8f)), 0, 'f', 1);
                emit referenceChanged(); emit frameChanged();
            }, Qt::QueuedConnection);
        } catch (const std::exception& error) {
            const auto message = QString::fromUtf8(error.what());
            QMetaObject::invokeMethod(this, [this, message] { referenceBusy_ = false; referenceInfo_ = message; emit referenceChanged(); }, Qt::QueuedConnection);
        }
    });
}
void AudioEngine::cancelReference() { cancelReference_ = true; }
void AudioEngine::clearReference() { if (!referenceBusy_) { reference_.reset(); referenceInfo_ = "No reference loaded"; emit referenceChanged(); emit frameChanged(); } }
QJsonObject AudioEngine::diagnostics() const {
    return {{"mode", demo_ ? "demo" : "loopback"}, {"state", status_}, {"device", device_},
        {"sampleRate", rate_}, {"capturedFrames", double(capture_.capturedFrames())},
        {"queueOverruns", double(capture_.overruns())}, {"analysisFrames", double(display_->totalFrames)},
        {"segments", double(display_->segments)}, {"spectrogramRows", display_->spectrogramRows},
        {"referenceLoaded", bool(reference_)}, {"referenceInfo", referenceInfo_}};
}
