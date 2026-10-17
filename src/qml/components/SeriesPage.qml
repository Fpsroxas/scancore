import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Details page shared by comics and novels: header, actions and a fast
// chapter list with read state, downloads and multi-select.
Item {
    id: page
    property string media: "comic"
    property string url
    property var summary: ({})
    property bool autoResume: false

    property var task: load()
    property bool descending: true
    property string filter: ""
    property bool selecting: false
    property var selection: ({})

    function load() {
        return media === "novel" ? Novels.book(url, summary) : Comics.series(url, summary)
    }

    readonly property var series: {
        const base = Object.assign({ media: media, sourceUrl: url }, summary)
        if (task && task.ok) return Object.assign(base, task.result.series)
        return base
    }
    readonly property string seriesId: series.id || (media === "novel" ? Novels.seriesId(url) : Comics.seriesId(url))
    // Until the site answers, the chapter list saved last time (if any).
    readonly property var storedChapters: Library.storedUnits(seriesId)
    readonly property var chapters: task && task.ok ? task.result.chapters : storedChapters
    readonly property var progress: { Library.revision; return Library.progressMap(seriesId) }
    readonly property var downloads: { Downloads.revision; return Downloads.states(seriesId) }
    readonly property bool saved: { Library.revision; return Library.isSaved(seriesId) }
    readonly property string coverReferer: media === "comic" ? Comics.imageReferer(url) : ""

    readonly property var resumeInfo: {
        Library.revision
        if (chapters.length === 0) return { index: 0, label: page.media === "novel" ? "Start reading" : "Start reading", detail: "" }
        const last = Library.lastProgress(seriesId)
        if (last && last.unitKey) {
            const i = chapters.findIndex(c => c.key === last.unitKey)
            if (i >= 0) {
                if (last.completed && i + 1 < chapters.length)
                    return { index: i + 1, label: "Continue", detail: "Next: " + chapters[i + 1].title }
                if (last.completed)
                    return { index: i, label: "Read again", detail: "You finished " + chapters[i].title }
                return { index: i, label: "Continue", detail: chapters[i].title + (last.percent > 0.02 ? " · " + Math.round(last.percent * 100) + "%" : "") }
            }
        }
        return { index: 0, label: "Start reading", detail: chapters[0].title }
    }

    readonly property var rows: {
        const out = []
        const f = filter.trim().toLowerCase()
        for (let i = 0; i < chapters.length; ++i) {
            const c = chapters[i]
            if (f && !(c.title || "").toLowerCase().includes(f) && String(c.number) !== f) continue
            out.push({ unit: c, index: i })
        }
        return descending ? out.reverse() : out
    }

    function openAt(index) {
        if (index < 0 || index >= chapters.length) return
        Library.markSeen(seriesId)
        if (media === "novel") Router.openNovelReader(series, chapters, index)
        else Router.openComicReader(series, chapters, index)
    }

    function toggleSave() {
        if (saved) {
            Library.remove(seriesId)
            Router.toast("Removed from your library")
        } else {
            Library.save(series)
            Router.toast("Added to your library", "success")
        }
    }

    function download(units) {
        if (units.length === 0) return
        Downloads.enqueue(series, units)
        Router.toast("Downloading " + units.length + (units.length === 1 ? " chapter" : " chapters"))
    }

    // Continue straight from the saved chapter list; the fresh one replaces
    // it on this page when it arrives.
    Component.onCompleted: {
        if (autoResume && storedChapters.length > 0) {
            autoResume = false
            Qt.callLater(() => page.openAt(page.resumeInfo.index))
        }
    }

    Connections {
        target: page.task
        function onFinished() {
            // The header grows once details arrive; keep the page at its top
            // unless the reader has already scrolled.
            if (!list.moving && list.contentY - list.originY < 40)
                Qt.callLater(() => list.positionViewAtBeginning())
            if (page.autoResume) {
                page.autoResume = false
                page.openAt(page.resumeInfo.index)
            }
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        model: page.rows
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 5000
        cacheBuffer: 1200
        reuseItems: true
        ScrollBar.vertical: ScScrollBar {}
        bottomMargin: 40

        Component.onCompleted: Qt.callLater(() => { if (headerItem) Router.shell.land(headerItem.details.coverItem) })

        header: Column {
            readonly property alias details: details
            width: list.width
            spacing: 0

            DetailsHeader {
                id: details
                series: page.series
                coverReferer: page.coverReferer
                kicker: (page.series.sourceLabel || page.series.source || "") + (page.media === "novel" ? " · Novel" : " · Comic")
                meta: [
                    page.series.status,
                    page.chapters.length ? page.chapters.length + (page.media === "novel" ? " chapters" : " chapters") : "",
                    page.series.type,
                    page.series.rating ? "★ " + page.series.rating : "",
                    page.task && page.task.ok && page.task.result.offline ? "Offline copy" : ""
                ]
                primaryText: page.resumeInfo.label
                primaryDetail: page.resumeInfo.detail
                busy: page.task ? page.task.loading && page.chapters.length === 0 : false
                saved: page.saved
                onPrimary: page.openAt(page.resumeInfo.index)
                onToggleSaved: page.toggleSave()

                IconButton {
                    iconName: "external-link"
                    filled: true
                    tip: "Open on the website"
                    onClicked: App.openUrl(page.url)
                }
                IconButton {
                    iconName: "refresh-cw"
                    filled: true
                    tip: "Refresh"
                    onClicked: page.task = page.load()
                }
            }

            // Toolbar
            Item {
                width: list.width
                height: 56
                visible: page.chapters.length > 0 || (page.task && page.task.loading)
                Rectangle {
                    x: Theme.gutter
                    width: parent.width - 2 * Theme.gutter
                    height: 1
                    color: Theme.border
                }
                Row {
                    x: Theme.gutter
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.chapters.length + (page.media === "novel" ? " chapters" : " chapters")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.textLg
                        font.weight: Font.DemiBold
                    }
                    SearchField {
                        visible: !Theme.compact
                        width: 200
                        implicitHeight: 34
                        placeholderText: "Find chapter"
                        onTextChanged: page.filter = text
                    }
                }
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.gutter - 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    ScButton {
                        visible: page.selecting
                        compact: true
                        variant: "primary"
                        iconName: "download"
                        text: "Download " + Object.keys(page.selection).length
                        enabled: Object.keys(page.selection).length > 0
                        onClicked: {
                            page.download(page.chapters.filter(c => page.selection[c.key]))
                            page.selecting = false
                            page.selection = ({})
                        }
                    }
                    ScButton {
                        visible: page.selecting
                        compact: true
                        text: "Unread"
                        onClicked: {
                            const s = {}
                            for (const c of page.chapters)
                                if (!(page.progress[c.key] && page.progress[c.key].completed)) s[c.key] = true
                            page.selection = s
                        }
                    }
                    ScButton {
                        visible: page.selecting
                        compact: true
                        text: "All"
                        onClicked: {
                            const s = {}
                            for (const c of page.chapters) s[c.key] = true
                            page.selection = s
                        }
                    }
                    IconButton {
                        iconName: page.selecting ? "x" : "download"
                        active: page.selecting
                        tip: page.selecting ? "Cancel" : "Select chapters to download"
                        onClicked: {
                            page.selecting = !page.selecting
                            page.selection = ({})
                        }
                    }
                    IconButton {
                        iconName: "check"
                        tip: "Mark all as read"
                        onClicked: Library.markUnits(page.series, page.chapters, true)
                    }
                    IconButton {
                        iconName: "arrow-down-wide-narrow"
                        rotation: page.descending ? 0 : 180
                        tip: page.descending ? "Newest first" : "Oldest first"
                        Behavior on rotation { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutCubic } }
                        onClicked: page.descending = !page.descending
                    }
                }
            }

            StateView {
                width: list.width
                height: visible ? 220 : 0
                task: page.task
                empty: page.task && page.task.ok && page.chapters.length === 0
                emptyTitle: "No chapters available"
                emptyText: "This source has not published readable chapters for this title."
                onRetry: page.task = page.load()
            }
            Text {
                visible: page.task && page.task.ok && page.task.result.offline === true
                x: Theme.gutter
                width: list.width - 2 * Theme.gutter
                bottomPadding: 10
                text: "Showing the saved chapter list. " + (page.task && page.task.ok ? (page.task.result.error || "") : "")
                color: Theme.warning
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                wrapMode: Text.Wrap
            }
        }

        delegate: UnitRow {
            required property var modelData
            width: list.width
            unit: modelData.unit
            progress: page.progress[modelData.unit.key] || null
            download: page.downloads[modelData.unit.key] || null
            current: page.resumeInfo.index === modelData.index
            selecting: page.selecting
            selected: page.selection[modelData.unit.key] === true
            onClicked: {
                if (page.selecting) {
                    const s = Object.assign({}, page.selection)
                    if (s[modelData.unit.key]) delete s[modelData.unit.key]
                    else s[modelData.unit.key] = true
                    page.selection = s
                } else {
                    page.openAt(modelData.index)
                }
            }
            onToggleRead: Library.markUnits(page.series, [modelData.unit], !(progress && progress.completed))
            onRequestDownload: page.download([modelData.unit])
            onRemoveDownload: Downloads.removeUnit(page.seriesId, modelData.unit.key)
        }

        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal }
        }
    }

    BackToTop { flick: list }
}
