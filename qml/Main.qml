import QtQuick

QtObject {
    id: root
    property var windows: []
    property var lastStates: []
    property bool replacing: false
    property Component windowComponent: Component { AnalyzerWindow {} }
    property Timer recoveryTimer: Timer {
        interval: 5000; running: true; repeat: true
        onTriggered: if (root.windows.length) workspace.remember(root.states())
    }

    function states() {
        return windows.map(function(w) { return w.savedState() })
    }
    function statesForShutdown() { return windows.length ? states() : lastStates }
    function openWindow(state) {
        if (windows.length >= 32) return
        let fitted = workspace.fitWindow(state || {})
        let w = windowComponent.createObject(root, {
            controller: root,
            analyzer: fitted.analyzer || "waveform",
            instanceId: fitted.instanceId || workspace.createId(),
            x: fitted.x, y: fitted.y, width: fitted.width, height: fitted.height,
            historySeconds: fitted.history === undefined ? 5 : fitted.history,
            gainDb: fitted.gain === undefined ? 0 : fitted.gain,
            floorDb: fitted.floor === undefined ? -110 : fitted.floor,
            slopeDb: fitted.slope === undefined ? 0 : fitted.slope,
            topmost: fitted.topmost || false,
            showReference: fitted.showReference === undefined ? true : fitted.showReference
        })
        if (w) {
            w.show()
            windows = windows.concat([w])
        }
    }
    function add(analyzer) {
        const offset = (windows.length % 7) * 34
        openWindow({analyzer: analyzer, x: 100 + offset, y: 90 + offset})
    }
    function removeWindow(w) {
        if (replacing || windows.indexOf(w) < 0) return
        if (windows.length === 1) {
            lastStates = states()
            workspace.remember(lastStates)
            windows = []
            Qt.quit()
        } else {
            windows = windows.filter(function(item) { return item !== w })
            workspace.remember(states())
        }
        w.destroy()
    }
    function saveLayout(name) { return workspace.saveLayout(name, states()) }
    function loadLayout(name) {
        const next = workspace.loadLayout(name)
        if (!next.length) return
        replacing = true
        const old = windows
        windows = []
        next.forEach(function(state) { root.openWindow(state) })
        old.forEach(function(w) { w.close(); w.destroy() })
        replacing = false
    }
    function bringIntoView() {
        windows.forEach(function(w) {
            const fit = workspace.fitWindow(w.savedState())
            w.x = fit.x; w.y = fit.y; w.width = fit.width; w.height = fit.height
            w.show(); w.raise()
        })
    }
    function quit() {
        lastStates = states()
        workspace.remember(lastStates)
        replacing = true
        Qt.quit()
    }
    Component.onCompleted: {
        workspace.initialWindows().forEach(function(state) { root.openWindow(state) })
    }
}
