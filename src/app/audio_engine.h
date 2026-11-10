#pragma once
#include "capture/wasapi_capture.h"
#include "dsp/analysis.h"
#include "dsp/wav_reader.h"
#include <QObject>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

class AudioEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY metricsChanged)
    Q_PROPERTY(QString device READ device NOTIFY metricsChanged)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY metricsChanged)
    Q_PROPERTY(qulonglong overruns READ overruns NOTIFY metricsChanged)
    Q_PROPERTY(QString peakText READ peakText NOTIFY metricsChanged)
    Q_PROPERTY(bool active READ active NOTIFY metricsChanged)
    Q_PROPERTY(bool referenceBusy READ referenceBusy NOTIFY referenceChanged)
    Q_PROPERTY(int referenceProgress READ referenceProgress NOTIFY referenceChanged)
    Q_PROPERTY(QString referenceInfo READ referenceInfo NOTIFY referenceChanged)
public:
    explicit AudioEngine(bool demo, QObject* parent = nullptr);
    ~AudioEngine() override;
    QString status() const { return status_; }
    QString device() const { return device_; }
    int sampleRate() const { return rate_; }
    qulonglong overruns() const { return capture_.overruns(); }
    QString peakText() const;
    bool active() const { return active_; }
    bool referenceBusy() const { return referenceBusy_; }
    int referenceProgress() const { return referenceProgress_; }
    QString referenceInfo() const;
    std::shared_ptr<const daa::AnalysisSnapshot> snapshot() const { return display_; }
    std::shared_ptr<const daa::ReferenceResult> reference() const { return reference_; }
    Q_INVOKABLE void importReference(const QUrl& url);
    Q_INVOKABLE void cancelReference();
    Q_INVOKABLE void clearReference();
    Q_INVOKABLE void setEco(bool enabled);
    QJsonObject diagnostics() const;
signals:
    void frameChanged();
    void metricsChanged();
    void referenceChanged();
private:
    void analyze();
    void tick();
    bool demo_;
    daa::AudioQueue queue_;
    daa::WasapiCapture capture_;
    std::atomic<bool> stopping_{false}, cancelReference_{false};
    std::thread worker_, referenceWorker_;
    std::mutex snapshotMutex_;
    std::shared_ptr<const daa::AnalysisSnapshot> latest_, display_;
    std::shared_ptr<const daa::ReferenceResult> reference_;
    QTimer timer_;
    QString status_ = "Starting", device_, referenceInfo_ = "No reference loaded";
    int rate_ = 48000, referenceProgress_ = 0;
    bool active_ = false, referenceBusy_ = false;
    int metricTicks_ = 0;
};
