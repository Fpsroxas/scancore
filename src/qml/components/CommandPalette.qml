import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Ctrl+K: jump anywhere, search any media type, reopen something recent, or
// run a quick command (theme, motion, update check, sync, shortcuts).
Popup {
    id: palette
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(640, parent.width - 80)
    height: Math.min(520, parent.height - 120)
    modal: true
    focus: true
    padding: 0
    property var recent: []
    property int selected: 0

    readonly property var actions: {
        const q = input.text.trim()
        const out = []
        if (q.length > 0) {
            out.push({ icon: "book-open", label: "Search comics for “" + q + "”", run: () => searchIn("comics", q) })
            out.push({ icon: "book-text", label: "Search novels for “" + q + "”", run: () => searchIn("novels", q) })
            out.push({ icon: "tv", label: "Search anime for “" + q + "”", run: () => searchIn("anime", q) })
        }
        const lower = q.toLowerCase()
        for (const entry of recent) {
            if (out.length > 14) break
            if (lower && !(entry.title || "").toLowerCase().includes(lower)) continue
            out.push({ icon: entry.media === "anime" ? "tv" : entry.media === "novel" ? "book-text" : "book-open",
                       label: entry.title, detail: entry.unitTitle || "", cover: entry.cover, run: () => Router.resume(entry) })
        }
        const pages = [["home", "Home", "house"], ["comics", "Comics", "book-open"], ["novels", "Novels", "book-text"],
                       ["anime", "Anime", "tv"], ["library", "Library", "library"], ["history", "History", "history"],
                       ["downloads", "Downloads", "download"], ["settings", "Settings", "settings"]]
        for (const [key, label, icon] of pages)
            if (!lower || label.toLowerCase().includes(lower))
                out.push({ icon: icon, label: "Go to " + label, run: () => Router.go(key) })
        const commands = [
            { icon: Theme.dark ? "sun" : "moon", label: Theme.dark ? "Switch to the light theme" : "Switch to the dark theme",
              run: () => App.setSetting("theme", Theme.dark ? "light" : "dark") },
            { icon: "sparkles", label: Motion.reduced ? "Turn animations back on" : "Reduce motion",
              run: () => App.setSetting("reducedMotion", !Motion.reduced) },
            { icon: "refresh-cw", label: "Check the library for new chapters", run: () => checkLibrary() }
        ]
        if (Account.signedIn)
            commands.push({ icon: "cloud", label: "Sync with scancore.onl", run: () => Account.sync() })
        if (App.platform !== "android")
            commands.push({ icon: "keyboard", label: "Keyboard shortcuts", detail: "?", run: () => Router.shell.openShortcuts() })
        for (const c of commands)
            if (!lower || c.label.toLowerCase().includes(lower))
                out.push(c)
        return out
    }

    // Held here so the check is not cancelled when the palette closes.
    property var checkTask: null
    function checkLibrary() {
        Router.toast("Checking your library for new chapters…")
        checkTask = Library.checkUpdates()
    }
    Connections {
        target: palette.checkTask
        function onFinished(result) {
            if (result.newUnits === 0) Router.toast("Everything is up to date")
        }
        function onFailed(error) { Router.toast(error, "error") }
    }

    function searchIn(section, q) {
        Router.go(section)
        Qt.callLater(() => {
            const stack = Router.shell.stackFor(section)
            if (stack && stack.depth > 1) stack.pop(null)
            const page = stack ? stack.get(0) : null
            if (page && page.search) page.search(q)
        })
    }

    onOpened: {
        input.text = ""
        selected = 0
        input.forceActiveFocus()
        historyTask = Library.history("", 30)
    }
    property var historyTask: null
    Connections {
        target: palette.historyTask
        function onFinished(result) { palette.recent = result }
    }

    function runSelected() {
        const action = actions[selected]
        if (!action) return
        close()
        action.run()
    }

    Overlay.modal: Rectangle {
        color: Theme.overlay
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast }
            NumberAnimation { property: "scale"; to: 0.97; duration: Motion.fast }
        }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
        border.width: 1
        border.color: Theme.borderStrong
    }

    contentItem: Column {
        TextField {
            id: input
            width: parent.width
            height: 60
            leftPadding: 54
            rightPadding: 20
            placeholderText: "Search comics, novels, anime or jump to a page"
            placeholderTextColor: Theme.textFaint
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.textLg
            background: Item {
                Icon {
                    x: 20
                    anchors.verticalCenter: parent.verticalCenter
                    name: "search"
                    size: 19
                    color: Theme.textMuted
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.border
                }
            }
            onTextChanged: palette.selected = 0
            Keys.onDownPressed: palette.selected = Math.min(palette.actions.length - 1, palette.selected + 1)
            Keys.onUpPressed: palette.selected = Math.max(0, palette.selected - 1)
            Keys.onReturnPressed: palette.runSelected()
            Keys.onEnterPressed: palette.runSelected()
            Keys.onEscapePressed: palette.close()
        }

        ListView {
            id: list
            width: parent.width
            height: palette.height - input.height
            clip: true
            topMargin: 8
            bottomMargin: 8
            model: palette.actions
            currentIndex: palette.selected
            highlightMoveDuration: Motion.fast
            highlightFollowsCurrentItem: true
            highlight: Rectangle {
                x: 8
                width: list.width - 16
                radius: Theme.radiusMd
                color: Theme.surface3
            }
            delegate: Item {
                required property var modelData
                required property int index
                width: list.width
                height: 46
                Row {
                    x: 22
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 14
                    Item {
                        width: 28
                        height: 28
                        anchors.verticalCenter: parent.verticalCenter
                        Cover {
                            visible: !!modelData.cover
                            anchors.fill: parent
                            radius: 6
                            source: modelData.cover || ""
                        }
                        Icon {
                            visible: !modelData.cover
                            anchors.centerIn: parent
                            name: modelData.icon
                            size: 17
                            color: Theme.textMuted
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: list.width - 160
                        text: modelData.label
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textMd
                        elide: Text.ElideRight
                    }
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 22
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    horizontalAlignment: Text.AlignRight
                    text: modelData.detail || ""
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: palette.selected = index
                    onClicked: {
                        palette.selected = index
                        palette.runSelected()
                    }
                }
            }
        }
    }
}
