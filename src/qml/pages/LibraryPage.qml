import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Saved comics, novels and anime, with update checks.
Item {
    id: page
    property string media: ""
    property string filter: ""
    property var task: Library.entries("")
    property var checkTask: null

    readonly property var entries: {
        const all = task && task.ok ? task.result : []
        const f = filter.trim().toLowerCase()
        return all.filter(e => (!media || e.media === media) && (!f || (e.title || "").toLowerCase().includes(f)))
    }

    function search(text) { filter = text }

    Connections {
        target: Library
        function onChanged() { refresh.restart() }
    }
    Timer { id: refresh; interval: 300; onTriggered: page.task = Library.entries("") }
    Connections {
        target: page.checkTask
        function onFinished(result) {
            Router.toast(result.newUnits > 0 ? result.newUnits + " new chapters found" : "Everything is up to date",
                         result.newUnits > 0 ? "success" : "info")
        }
        function onFailed(error) { Router.toast(error, "error") }
    }

    SmoothFlickable {
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: 22

            PageHeader {
                title: "Library"
                subtitle: (page.task && page.task.ok ? page.task.result.length : 0) + " saved"
                    + (Library.unreadUpdates > 0 ? " · " + Library.unreadUpdates + " new chapters" : "")
                ScButton {
                    text: page.checkTask && page.checkTask.loading ? "Checking…" : "Check for updates"
                    iconName: "refresh-cw"
                    loading: page.checkTask ? page.checkTask.loading : false
                    onClicked: page.checkTask = Library.checkUpdates()
                }
            }

            Flow {
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 12
                Segmented {
                    options: [{ value: "", label: "All" }, { value: "comic", label: "Comics", icon: "book-open" },
                              { value: "novel", label: "Novels", icon: "book-text" }, { value: "anime", label: "Anime", icon: "tv" }]
                    value: page.media
                    onPicked: (v) => page.media = v
                }
                SearchField {
                    width: Theme.compact ? parent.width : 260
                    placeholderText: "Filter library"
                    text: page.filter
                    onTextChanged: page.filter = text
                }
            }

            CoverGrid {
                items: page.entries
                badgeFor: (it) => it.newUnits > 0 ? "+" + it.newUnits : ""
                progressFor: (it) => it.unitKey ? (it.completed ? 1 : (it.percent || 0)) : -1
                subtitleFor: (it) => it.unitTitle
                    ? (it.media === "anime" ? "Episode " + (it.unitNumber || it.unitKey) : it.unitTitle)
                    : (it.media === "anime" ? "Anime" : it.media === "novel" ? "Novel" : "Comic")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }

            StateView {
                width: parent.width
                task: page.task
                empty: page.task && page.task.ok && page.entries.length === 0
                emptyTitle: page.filter || page.media ? "Nothing matches" : "Your library is empty"
                emptyText: "Open any comic, novel or anime and choose “Add to library” to keep it here. SCANCORE checks saved series for new chapters in the background."
                emptyIcon: "library"
                onRetry: page.task = Library.entries("")
            }
        }
    }
}
