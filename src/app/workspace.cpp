#include "app/workspace.h"
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QScreen>
#include <QUuid>
#include <algorithm>

Workspace::Workspace(QString folder, QObject* parent) : QObject(parent), folder_(std::move(folder)) {
    data_ = {{"schemaVersion", 1}, {"layouts", QJsonObject{}}, {"startupLayout", ""}, {"restoreLast", false}};
    QFile file(filePath());
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        corrupt_ = true; message_ = "Cannot read saved workspace; using a waveform window"; return;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() ||
        document.object().value("schemaVersion").toInt() != 1 || !document.object().value("layouts").isObject()) {
        corrupt_ = true; message_ = "Saved workspace is invalid; original file preserved"; return;
    }
    data_ = document.object();
}
QStringList Workspace::layoutNames() const { return data_.value("layouts").toObject().keys(); }
QString Workspace::createId() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QVariantMap Workspace::fitWindow(const QVariantMap& input) const {
    auto value = input;
    auto* screen = QGuiApplication::primaryScreen();
    for (auto* candidate : QGuiApplication::screens())
        if (candidate->name() == value.value("screen").toString()) { screen = candidate; break; }
    if (!screen) return value;
    const auto bounds = screen->availableGeometry();
    const int width = std::clamp(value.value("width", 980).toInt(), std::min(600, bounds.width()), bounds.width());
    const int height = std::clamp(value.value("height", 430).toInt(), std::min(300, bounds.height()), bounds.height());
    value["width"] = width; value["height"] = height;
    value["x"] = std::clamp(value.value("x", bounds.x() + 80).toInt(), bounds.left(), bounds.right() - width + 1);
    value["y"] = std::clamp(value.value("y", bounds.y() + 80).toInt(), bounds.top() + 30, std::max(bounds.top() + 30, bounds.bottom() - height - 30));
    value["screen"] = screen->name();
    return value;
}
QVariantList Workspace::validWindows(const QVariantList& windows) const {
    QVariantList result;
    for (const auto& input : windows) {
        if (result.size() >= 32) break;
        auto window = input.toMap();
        const auto analyzer = window.value("analyzer", "waveform").toString();
        if (analyzer != "waveform" && analyzer != "spectrum" && analyzer != "spectrogram") continue;
        window["analyzer"] = analyzer;
        if (window.value("instanceId").toString().isEmpty()) window["instanceId"] = createId();
        window["history"] = std::clamp(window.value("history", 5).toDouble(), 1.0, 10.0);
        window["gain"] = std::clamp(window.value("gain", 0).toDouble(), -24.0, 24.0);
        window["floor"] = std::clamp(window.value("floor", -110).toDouble(), -140.0, -30.0);
        window["slope"] = std::clamp(window.value("slope", 0).toDouble(), 0.0, 6.0);
        result.append(fitWindow(window));
    }
    return result;
}
bool Workspace::write() {
    if (!QDir().mkpath(folder_)) { message_ = "Cannot create workspace folder"; emit changed(); return false; }
    // Preserve an invalid/future-version file before allowing a new save.
    if (corrupt_ && QFile::exists(filePath())) {
        const auto preserved = filePath() + ".invalid-" + createId();
        if (!QFile::copy(filePath(), preserved)) { message_ = "Cannot preserve invalid workspace"; emit changed(); return false; }
        corrupt_ = false;
    } else if (QFile existing(filePath()); existing.open(QIODevice::ReadOnly)) {
        QSaveFile backup(filePath() + ".bak");
        if (!backup.open(QIODevice::WriteOnly) || backup.write(existing.readAll()) < 0 || !backup.commit()) {
            message_ = "Cannot create workspace backup"; emit changed(); return false;
        }
    }
    QSaveFile file(filePath());
    const auto bytes = QJsonDocument(data_).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        message_ = "Cannot save workspace"; emit changed(); return false;
    }
    return true;
}
bool Workspace::saveLayout(const QString& rawName, const QVariantList& windows) {
    const auto name = rawName.trimmed().left(80);
    const auto validated = validWindows(windows);
    if (name.isEmpty() || validated.isEmpty()) { message_ = "Enter a layout name and keep at least one window open"; emit changed(); return false; }
    auto layouts = data_.value("layouts").toObject(); layouts[name] = QJsonArray::fromVariantList(validated);
    data_["layouts"] = layouts;
    const auto saved = write(); if (saved) message_ = "Saved layout: " + name;
    emit changed(); return saved;
}
QVariantList Workspace::loadLayout(const QString& name) {
    const auto windows = data_.value("layouts").toObject().value(name);
    if (!windows.isArray()) { message_ = "Layout not found"; emit changed(); return {}; }
    return validWindows(windows.toArray().toVariantList());
}
bool Workspace::deleteLayout(const QString& name) {
    auto layouts = data_.value("layouts").toObject(); layouts.remove(name); data_["layouts"] = layouts;
    if (startupLayout() == name) data_["startupLayout"] = "";
    const auto saved = write(); message_ = saved ? "Deleted layout: " + name : message_; emit changed(); return saved;
}
void Workspace::setStartupLayout(const QString& name) { data_["startupLayout"] = name; write(); emit changed(); }
void Workspace::setRestoreLast(bool value) { data_["restoreLast"] = value; write(); emit changed(); }
void Workspace::remember(const QVariantList& windows) {
    const auto value = QJsonArray::fromVariantList(validWindows(windows));
    if (data_.value("lastWorkspace") == value) return;
    data_["lastWorkspace"] = value; write();
}
QVariantList Workspace::initialWindows() {
    QVariantList result;
    if (restoreLast()) result = validWindows(data_.value("lastWorkspace").toArray().toVariantList());
    else if (!startupLayout().isEmpty()) result = loadLayout(startupLayout());
    if (result.isEmpty()) result = validWindows({QVariantMap{{"analyzer", "waveform"}, {"instanceId", createId()}}});
    return result;
}
