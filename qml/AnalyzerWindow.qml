import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import DesktopAnalyzer 1.0

ApplicationWindow {
    id: win
    property var controller
    property string instanceId
    property string analyzer: "waveform"
    property real historySeconds: 5
    property real gainDb: 0
    property real floorDb: -110
    property real slopeDb: 0
    property bool topmost: false
    property bool showReference: true
    property bool held: false
    readonly property string analyzerTitle: analyzer === "waveform" ? "Waveform" : (analyzer === "spectrum" ? "Spectrum" : "Spectrogram")
    readonly property real maximumHz: Math.min(20000, audio.sampleRate / 2)
    visible: false
    width: 980; height: 430
    minimumWidth: 640; minimumHeight: 300
    title: analyzerTitle + " - Desktop Audio Analyzer"
    color: "#10151c"
    flags: Qt.Window | (topmost ? Qt.WindowStaysOnTopHint : 0)
    palette.window: "#10151c"
    palette.windowText: "#e2e9f2"
    palette.base: "#19232e"
    palette.text: "#e2e9f2"
    palette.button: "#24313e"
    palette.buttonText: "#e2e9f2"
    palette.highlight: "#438c7a"
    palette.highlightedText: "#ffffff"
    font.family: "Segoe UI"
    font.pixelSize: 12

    function savedState() {
        return {instanceId: instanceId, analyzer: analyzer, x: x, y: y, width: width, height: height,
            screen: screen ? screen.name : "", history: historySeconds, gain: gainDb, floor: floorDb,
            slope: slopeDb, topmost: topmost, showReference: showReference}
    }
    onClosing: controller.removeWindow(win)
    Shortcut { sequence: "Ctrl+N"; onActivated: addMenu.open() }
    Shortcut { sequence: "Ctrl+S"; onActivated: layoutDialog.open() }
    Shortcut { sequence: "Space"; onActivated: win.held = !win.held }
    Shortcut { sequence: "Ctrl+Q"; onActivated: controller.quit() }

    header: Rectangle {
        height: 58; color: "#172029"
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 14; spacing: 12
            Rectangle { width: 7; height: 25; radius: 3; color: "#74e3c7" }
            ColumnLayout {
                spacing: 2; Layout.fillWidth: true
                Label { text: win.analyzerTitle; font.pixelSize: 16; font.weight: Font.DemiBold }
                Label { text: win.analyzer === "waveform" ? "STEREO AMPLITUDE HISTORY" : (win.analyzer === "spectrum" ? "STEREO ENERGY SPECTRUM" : "TIME / FREQUENCY HISTORY"); color: "#8799ab"; font.pixelSize: 9; font.letterSpacing: 1.2 }
            }
            Button {
                text: "+ Analyzer"; onClicked: addMenu.open()
                Menu {
                    id: addMenu; y: parent.height
                    MenuItem { text: "Waveform"; onTriggered: controller.add("waveform") }
                    MenuItem { text: "Spectrum"; onTriggered: controller.add("spectrum") }
                    MenuItem { text: "Spectrogram"; onTriggered: controller.add("spectrogram") }
                }
            }
            Button { text: "Layouts"; onClicked: layoutDialog.open() }
            Button { text: win.held ? "Resume" : "Hold"; highlighted: win.held; onClicked: win.held = !win.held }
            Button { text: "Settings"; onClicked: settings.open() }
        }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 8
        RowLayout {
            spacing: 8
            Rectangle { width: 6; height: 6; radius: 3; color: audio.active ? "#74e3c7" : "#f4bb7b" }
            Label { text: audio.status; color: "#b5c5d4"; font.pixelSize: 11 }
            Label { text: " / " + audio.device; color: "#7f93a7"; elide: Text.ElideRight; Layout.fillWidth: true; font.pixelSize: 11 }
            Label { text: audio.sampleRate / 1000 + " kHz"; color: "#7f93a7"; font.pixelSize: 11 }
            Label { text: audio.peakText; color: "#b5c5d4"; font.family: "Consolas"; font.pixelSize: 11 }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; color: "#0c1118"; border.color: "#26313f"; radius: 4
            Item {
                anchors.fill: parent; anchors.leftMargin: 48; anchors.rightMargin: 16; anchors.topMargin: 20; anchors.bottomMargin: 26
                PlotItem {
                    id: plot; anchors.fill: parent; clip: true; engine: audio
                    analyzer: win.analyzer; history: win.historySeconds; gain: win.gainDb; floor: win.floorDb
                    slope: win.slopeDb; held: win.held; showReference: win.showReference
                }
                Label { visible: win.analyzer === "waveform"; text: "L"; x: -30; y: parent.height * 0.25 - 7; color: "#74e3c7"; font.pixelSize: 10 }
                Label { visible: win.analyzer === "waveform"; text: "R"; x: -30; y: parent.height * 0.75 - 7; color: "#7ba9f7"; font.pixelSize: 10 }
                Repeater {
                    model: win.analyzer === "spectrum" ? [0, -20, -40, -60, -80, -100, -120] : []
                    Label { required property int modelData; text: modelData; visible: modelData >= win.floorDb; x: -34; y: parent.height * (modelData / win.floorDb) - 6; color: "#6f8399"; font.family: "Consolas"; font.pixelSize: 10 }
                }
                Repeater {
                    model: win.analyzer === "waveform" ? [] : [20, 100, 1000, 10000, 20000]
                    Label {
                        required property int modelData
                        text: modelData >= 1000 ? (modelData / 1000 + "k") : modelData
                        visible: modelData <= win.maximumHz
                        x: Math.log(modelData / 20) / Math.log(win.maximumHz / 20) * parent.width - width / 2
                        y: parent.height + 8; color: "#7f93a7"; font.family: "Consolas"; font.pixelSize: 10
                    }
                }
                Label { visible: win.analyzer === "waveform"; text: "-" + win.historySeconds.toFixed(1) + " s"; y: parent.height + 8; color: "#7f93a7"; font.family: "Consolas"; font.pixelSize: 10 }
                Label { visible: win.analyzer === "waveform"; text: "NOW"; anchors.right: parent.right; y: parent.height + 8; color: "#7f93a7"; font.pixelSize: 10 }
                Label { visible: win.analyzer === "spectrogram"; text: "PAST"; x: -38; color: "#7f93a7"; font.pixelSize: 9 }
                Label { visible: win.analyzer === "spectrogram"; text: "NOW"; x: -38; anchors.bottom: parent.bottom; color: "#7f93a7"; font.pixelSize: 9 }
                Column {
                    anchors.centerIn: parent; spacing: 6; visible: !audio.active && !win.held
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Waiting for desktop audio"; color: "#b5c5d4"; font.pixelSize: 16 }
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Play audio through your default Windows output"; color: "#6f8399"; font.pixelSize: 11 }
                }
                Label { visible: win.held; text: "DISPLAY HELD"; anchors.right: parent.right; color: "#f4bb7b"; font.pixelSize: 10; font.letterSpacing: 1 }
            }
        }
        RowLayout {
            Label {
                text: win.analyzer === "waveform" ? win.historySeconds.toFixed(1) + " s history  /  " + win.gainDb.toFixed(0) + " dB display gain"
                    : "4096 FFT  /  Hann  /  75% overlap  /  dBFS/Hz" + (win.analyzer === "spectrum" ? "  /  " + win.slopeDb.toFixed(1) + " dB/oct" : "")
                color: "#72889e"; font.pixelSize: 10; Layout.fillWidth: true
            }
            Label { text: audio.overruns + " overruns"; color: audio.overruns ? "#f38172" : "#62788d"; font.pixelSize: 10 }
        }
        RowLayout {
            visible: win.analyzer === "spectrum"; spacing: 8
            Rectangle { width: 16; height: 2; color: "#f4bb7b" }
            Label { text: audio.referenceInfo; color: "#af9b83"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
            Button { text: audio.referenceBusy ? "Cancel " + audio.referenceProgress + "%" : "Import WAV reference"; font.pixelSize: 10; onClicked: audio.referenceBusy ? audio.cancelReference() : referenceDialog.open() }
        }
    }

    Drawer {
        id: settings; edge: Qt.RightEdge; width: 300; height: win.height
        background: Rectangle { color: "#17212c"; border.color: "#354556" }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 22; spacing: 12
            Label { text: win.analyzerTitle + " settings"; font.pixelSize: 17; font.weight: Font.DemiBold }
            Label { text: "Changes apply to this window"; color: "#7f93a7"; font.pixelSize: 11 }
            Label { visible: win.analyzer !== "spectrum"; text: "History  /  " + win.historySeconds.toFixed(1) + " s" }
            Slider { visible: win.analyzer !== "spectrum"; Layout.fillWidth: true; from: 1; to: 10; stepSize: 0.5; value: win.historySeconds; onMoved: win.historySeconds = value }
            Label { text: "Display gain  /  " + win.gainDb.toFixed(0) + " dB" }
            Slider { Layout.fillWidth: true; from: -24; to: 24; stepSize: 1; value: win.gainDb; onMoved: win.gainDb = value }
            Label { visible: win.analyzer !== "waveform"; text: "Display floor  /  " + win.floorDb.toFixed(0) + " dBFS/Hz" }
            Slider { visible: win.analyzer !== "waveform"; Layout.fillWidth: true; from: -140; to: -40; stepSize: 5; value: win.floorDb; onMoved: win.floorDb = value }
            Label { visible: win.analyzer === "spectrum"; text: "Display slope  /  " + win.slopeDb.toFixed(1) + " dB/oct" }
            Slider { visible: win.analyzer === "spectrum"; Layout.fillWidth: true; from: 0; to: 6; stepSize: 0.5; value: win.slopeDb; onMoved: win.slopeDb = value }
            CheckBox { text: "Always on top"; checked: win.topmost; onToggled: win.topmost = checked }
            CheckBox { visible: win.analyzer === "spectrum"; text: "Show reference curve"; checked: win.showReference; onToggled: win.showReference = checked }
            Button { visible: win.analyzer === "spectrum"; text: "Clear reference"; enabled: !audio.referenceBusy; onClicked: audio.clearReference() }
            Item { Layout.fillHeight: true }
            Label { visible: win.analyzer !== "waveform"; text: "FFT spacing: " + (audio.sampleRate / 4096).toFixed(2) + " Hz\nTransform duration: " + (4096000 / audio.sampleRate).toFixed(1) + " ms"; color: "#7f93a7"; font.pixelSize: 11 }
            Button { text: "Done"; Layout.fillWidth: true; onClicked: settings.close() }
        }
    }

    Dialog {
        id: layoutDialog; title: "Window layouts"; modal: true
        anchors.centerIn: parent; width: 420; standardButtons: Dialog.Close
        ColumnLayout {
            width: parent.width; spacing: 12
            TextField { id: layoutName; placeholderText: "Name this layout"; Layout.fillWidth: true; maximumLength: 80 }
            Button { text: "Save current windows"; Layout.fillWidth: true; onClicked: controller.saveLayout(layoutName.text) }
            ComboBox { id: layoutChoice; model: workspace.layoutNames; Layout.fillWidth: true }
            RowLayout {
                Button { text: "Load"; enabled: layoutChoice.count > 0; onClicked: { controller.loadLayout(layoutChoice.currentText) } }
                Button { text: "Delete"; enabled: layoutChoice.count > 0; onClicked: workspace.deleteLayout(layoutChoice.currentText) }
            }
            Label { text: "At startup"; color: "#9baebf" }
            ComboBox {
                Layout.fillWidth: true
                model: {
                    let names = ["Waveform only"]
                    for (let i = 0; i < workspace.layoutNames.length; ++i) names.push(workspace.layoutNames[i])
                    return names
                }
                currentIndex: Math.max(0, model.indexOf(workspace.startupLayout))
                onActivated: workspace.startupLayout = currentIndex === 0 ? "" : currentText
            }
            CheckBox { text: "Restore last session instead"; checked: workspace.restoreLast; onToggled: workspace.restoreLast = checked }
            Button { text: "Bring all windows into view"; Layout.fillWidth: true; onClicked: controller.bringIntoView() }
            Label { text: workspace.message; color: "#74e3c7"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
        }
    }
    FileDialog {
        id: referenceDialog; title: "Import a spectrum reference"
        nameFilters: ["WAV audio (*.wav)"]
        onAccepted: audio.importReference(selectedFile)
    }
    DropArea {
        anchors.fill: parent
        enabled: win.analyzer === "spectrum" && !audio.referenceBusy
        onDropped: function(drop) { if (drop.hasUrls && drop.urls.length) { audio.importReference(drop.urls[0]); drop.acceptProposedAction() } }
    }
}
