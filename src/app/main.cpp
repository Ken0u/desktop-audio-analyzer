#include "app/audio_engine.h"
#include "app/plot_item.h"
#include "app/workspace.h"
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJSValue>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <map>
#include <vector>

int main(int argc, char* argv[]) {
    if (!qEnvironmentVariableIsSet("QSG_RENDER_LOOP")) qputenv("QSG_RENDER_LOOP", "basic");
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) qputenv("QSG_RHI_BACKEND", "opengl");
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication app(argc, argv);
    app.setApplicationName("DesktopAudioAnalyzer"); app.setOrganizationName("DesktopAudioAnalyzer");
    app.setApplicationVersion("0.1.0"); app.setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle("Basic");
    QCommandLineParser parser;
    parser.setApplicationDescription("Desktop audio analysis - first milestone prototype");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOptions({
        {"demo", "Use an internal deterministic test signal instead of desktop capture"},
        {"data-dir", "Override the workspace storage folder", "folder"},
        {"windows", "Open this many windows for a performance smoke test (1-32)", "count", "1"},
        {"quit-after", "Exit after the given number of seconds", "seconds"},
        {"report", "Write JSON diagnostics when exiting", "file"},
        {"screenshot", "Save an image of the first window before exiting", "file"},
        {"reference", "Analyze a WAV spectrum reference on startup", "file"},
        {"exercise-layout", "Save, reload, and validate a smoke-test layout"}
    });
    parser.process(app);
    auto folder = parser.value("data-dir");
    if (folder.isEmpty()) folder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    Workspace workspace(folder);
    AudioEngine audio(parser.isSet("demo"));
    qmlRegisterType<PlotItem>("DesktopAnalyzer", 1, 0, "PlotItem");
    qmlRegisterUncreatableType<AudioEngine>("DesktopAnalyzer", 1, 0, "AudioEngine", "Use the shared engine");
    struct FrameTiming {
        std::chrono::steady_clock::time_point previous{};
        std::vector<double> intervals;
        int count = 0;
    };
    // These objects outlive QML windows and their destruction callbacks.
    std::map<QQuickWindow*, std::shared_ptr<FrameTiming>> timings;
    QTimer probeTimer;
    bool qmlFailed = false;
    QStringList qmlErrors;
    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty("audio", &audio);
    qml.rootContext()->setContextProperty("workspace", &workspace);
    QObject::connect(&qml, &QQmlEngine::warnings, &app, [&](const QList<QQmlError>& errors) {
        for (const auto& error : errors) qmlErrors.append(error.toString());
    });
    QObject::connect(&qml, &QQmlApplicationEngine::objectCreationFailed, &app, [&] { qmlFailed = true; app.exit(1); }, Qt::QueuedConnection);
    QObject::connect(&qml, &QQmlEngine::quit, &app, &QCoreApplication::quit);
    qml.load(QUrl("qrc:/qml/Main.qml"));
    if (qml.rootObjects().isEmpty()) return 1;
    auto* root = qml.rootObjects().first();
    const auto windowStates = [root]() {
        QVariant value; QMetaObject::invokeMethod(root, "statesForShutdown", Q_RETURN_ARG(QVariant, value));
        if (value.metaType() == QMetaType::fromType<QJSValue>()) return value.value<QJSValue>().toVariant().toList();
        return value.toList();
    };
    const auto count = std::clamp(parser.value("windows").toInt(), 1, 32);
    for (int i = static_cast<int>(windowStates().size()); i < count; ++i) {
        const auto analyzer = i % 3 == 1 ? "spectrum" : (i % 3 == 2 ? "spectrogram" : "waveform");
        QMetaObject::invokeMethod(root, "add", Q_ARG(QVariant, QString(analyzer)));
    }
    if (parser.isSet("report")) {
        QObject::connect(&probeTimer, &QTimer::timeout, &app, [&] {
            for (auto* window : root->findChildren<QQuickWindow*>()) {
                if (!window->property("analyzer").isValid() || timings.contains(window)) continue;
                auto timing = std::make_shared<FrameTiming>(); timings[window] = timing;
                QObject::connect(window, &QQuickWindow::frameSwapped, &app, [timing] {
                    const auto now = std::chrono::steady_clock::now();
                    if (timing->count && timing->intervals.size() < 10000)
                        timing->intervals.push_back(std::chrono::duration<double, std::milli>(now - timing->previous).count());
                    timing->previous = now; ++timing->count;
                });
                QObject::connect(window, &QObject::destroyed, &app, [&, window] { timings.erase(window); });
            }
        });
        probeTimer.start(250);
    }
    if (parser.isSet("reference")) audio.importReference(QUrl::fromLocalFile(QFileInfo(parser.value("reference")).absoluteFilePath()));
    bool layoutPassed = !parser.isSet("exercise-layout");
    if (parser.isSet("exercise-layout")) {
        QTimer::singleShot(500, &app, [&] {
            const auto states = windowStates();
            layoutPassed = workspace.saveLayout("Smoke test", states) && workspace.loadLayout("Smoke test").size() == count;
            if (layoutPassed) QMetaObject::invokeMethod(root, "loadLayout", Q_ARG(QVariant, QString("Smoke test")));
        });
    }
    bool screenshotSaved = false;
    QJsonArray renderedWindows;
    const auto captureScreenshots = [&] {
        if (!parser.isSet("screenshot")) return;
        const QFileInfo target(parser.value("screenshot"));
        int index = 0;
        for (auto* object : root->findChildren<QQuickWindow*>()) {
            if (!object->isVisible()) continue;
            const auto path = index == 0 ? target.filePath() : target.path() + "/" + target.completeBaseName()
                + "-" + QString::number(index) + "-" + object->property("analyzer").toString() + ".png";
            const auto saved = object->grabWindow().save(path);
            if (index == 0) screenshotSaved = saved;
            ++index;
        }
    };
    if (parser.isSet("quit-after")) {
        const auto seconds = std::clamp(parser.value("quit-after").toDouble(), 0.5, 3600.0);
        QTimer::singleShot(static_cast<int>(seconds * 1000), &app, [&] {
            for (const auto& [window, timing] : timings) {
                auto intervals = timing->intervals;
                std::sort(intervals.begin(), intervals.end());
                const auto p95 = intervals.empty() ? 0.0 : intervals[static_cast<std::size_t>((intervals.size() - 1) * 0.95)];
                renderedWindows.append(QJsonObject{{"analyzer", window->property("analyzer").toString()},
                    {"exposed", window->isExposed()}, {"frameSwaps", timing->count},
                    {"observedFrameSwapP95Ms", p95}, {"devicePixelRatio", window->devicePixelRatio()}});
            }
            captureScreenshots(); QMetaObject::invokeMethod(root, "quit");
        });
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
        const auto states = windowStates();
        workspace.remember(states);
        if (parser.isSet("report")) {
            auto report = audio.diagnostics();
            report["graphicsBackend"] = QString::fromUtf8(qgetenv("QSG_RHI_BACKEND")) + " / " + QString::fromUtf8(qgetenv("QSG_RENDER_LOOP"));
            report["renderedWindows"] = renderedWindows;
            report["windowStates"] = QJsonArray::fromVariantList(states);
            report["windowCount"] = states.size(); report["layoutRoundTripPassed"] = layoutPassed;
            report["qmlErrors"] = QJsonArray::fromStringList(qmlErrors); report["screenshotSaved"] = screenshotSaved;
            QFile file(parser.value("report"));
            if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(report).toJson());
        }
    });
    const auto result = app.exec();
    probeTimer.stop();
    return qmlFailed || !layoutPassed || !qmlErrors.isEmpty() ? 1 : result;
}
