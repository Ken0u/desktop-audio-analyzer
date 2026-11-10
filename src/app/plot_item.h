#pragma once
#include "app/audio_engine.h"
#include <QQuickItem>

class PlotItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(AudioEngine* engine READ engine WRITE setEngine NOTIFY settingsChanged)
    Q_PROPERTY(QString analyzer MEMBER analyzer_ NOTIFY settingsChanged)
    Q_PROPERTY(double history MEMBER history_ NOTIFY settingsChanged)
    Q_PROPERTY(double gain MEMBER gain_ NOTIFY settingsChanged)
    Q_PROPERTY(double floor MEMBER floor_ NOTIFY settingsChanged)
    Q_PROPERTY(double slope MEMBER slope_ NOTIFY settingsChanged)
    Q_PROPERTY(bool held READ held WRITE setHeld NOTIFY settingsChanged)
    Q_PROPERTY(bool showReference MEMBER showReference_ NOTIFY settingsChanged)
public:
    explicit PlotItem(QQuickItem* parent = nullptr);
    AudioEngine* engine() const { return engine_; }
    void setEngine(AudioEngine* value);
    bool held() const { return held_; }
    void setHeld(bool held);
signals:
    void settingsChanged();
protected:
    QSGNode* updatePaintNode(QSGNode* previous, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& next, const QRectF& previous) override;
private:
    AudioEngine* engine_ = nullptr;
    QString analyzer_ = "waveform";
    double history_ = 5, gain_ = 0, floor_ = -110, slope_ = 0;
    bool held_ = false, showReference_ = true;
    std::shared_ptr<const daa::AnalysisSnapshot> heldSnapshot_;
};
