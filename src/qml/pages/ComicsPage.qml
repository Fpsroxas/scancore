import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Comic search across every source, plus the latest chapters when idle.
Item {
    id: page
    property string query: ""
    property string source: App.settings.comicSource || "all"
    property var searchTask: null
    property int latestLimit: 36
    property var latestTask: Comics.latest(latestLimit)
    property var historyTask: Library.history("comic", 16)
    property var history: []
    Connections {
        target: page.historyTask
        function onFinished(result) { page.history = Router.keep(page.history, result) }
    }
    Connections {
        target: Library
        function onChanged() { refreshHistory.restart() }
    }
    Timer { id: refreshHistory; interval: 500; onTriggered: page.historyTask = Library.history("comic", 16) }
    // Files on this device are listed without a search.
    // "All sources" searches Comick's catalogue, then finds the pick on a source.
    readonly property bool useCatalog: source === "all" && App.settings.comickCatalog !== false
    // The catalogue could not be reached, so the sources were searched instead.
    property bool catalogFailed: false
    readonly property bool catalogResults: !!searchTask && searchTask.ok && !catalogFailed && useCatalog && query.length > 0
    readonly property bool browsingLocal: source === "local" && query.length === 0
    readonly property bool searching: query.length > 0 || source === "local"
    Component.onCompleted: if (source === "local") run("")

    function search(text) {
        field.text = text
        run(text)
    }

    function run(text) {
        const trimmed = text.trim()
        // A pasted series link opens that series directly.
        if (/^https?:\/\//.test(trimmed) && Comics.sourceForUrl(trimmed)) {
            Router.openSeries({ media: "comic", sourceUrl: trimmed, title: "", source: Comics.sourceForUrl(trimmed) }, null)
            return
        }
        query = trimmed
        if (query.length > 0) recent.remember(query)
        catalogFailed = false
        searchTask = query.length > 0 && useCatalog ? Comics.catalog(query)
            : query.length > 0 || source === "local" ? Comics.search(query, source) : null
        flick.scrollTo(0)
    }

    Connections {
        target: page.searchTask
        function onFailed() {
            if (!page.useCatalog || page.catalogFailed || page.query.length === 0) return
            page.catalogFailed = true
            page.searchTask = Comics.search(page.query, "all")
        }
    }

    ReadFromDialog { id: readFrom }

    readonly property var sources: Comics.sources().filter(s => s.enabled && (!s.nsfw || App.settings.showNsfwSources))

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: 26

            PageHeader {
                title: qsTr("Comics")
                subtitle: page.sources.length + qsTr(" sources · manhwa, manga and webtoons")
            }

            SearchField {
                id: field
                x: Theme.gutter
                width: Math.min(parent.width - 2 * Theme.gutter, 760)
                large: true
                placeholderText: page.useCatalog ? qsTr("Search every manhwa, manga and comic") : qsTr("Search titles across every source")
                onSubmitted: (text) => page.run(text)
                onTextChanged: if (text.length === 0 && page.query.length > 0) page.run("")
            }

            ChipRow {
                Chip {
                    text: qsTr("All sources")
                    selected: page.source === "all"
                    onClicked: {
                        page.source = "all"
                        App.setSetting("comicSource", "all")
                        page.run(page.query)
                    }
                }
                Repeater {
                    model: page.sources
                    Chip {
                        required property var modelData
                        text: modelData.label
                        selected: page.source === modelData.key
                        detail: {
                            if (!page.searchTask || !page.searchTask.ok) return ""
                            const p = page.searchTask.result.providers.find(x => x.source === modelData.key)
                            return p ? (p.ok ? String(p.count) : "!") : ""
                        }
                        onClicked: {
                            page.source = modelData.key
                            App.setSetting("comicSource", modelData.key)
                            if (page.searching || modelData.key === "local") page.run(page.query)
                        }
                    }
                }
            }

            // ---------------------------------------------------- results
            Column {
                visible: page.searching
                width: parent.width
                spacing: 18

                Item {
                    width: parent.width
                    height: 24
                    visible: page.searchTask && page.searchTask.ok
                    Text {
                        x: Theme.gutter
                        text: !page.searchTask || !page.searchTask.ok ? ""
                            : page.browsingLocal ? page.searchTask.result.comics.length + qsTr(" on this device")
                            : page.catalogResults ? page.searchTask.result.comics.length + qsTr(" titles for “") + page.query + qsTr("” from Comick's catalogue")
                            : page.searchTask.result.comics.length + qsTr(" results for “") + page.query + "”"
                                + (page.catalogFailed ? qsTr(" · Comick is unavailable, so your sources were searched") : "")
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.gutter
                        readonly property var failed: page.searchTask && page.searchTask.ok
                            ? page.searchTask.result.providers.filter(p => !p.ok) : []
                        visible: failed.length > 0
                        text: (failed.length === 1 ? qsTr("%1 is unavailable") : qsTr("%1 are unavailable")).arg(failed.map(p => p.label).join(", "))
                        color: Theme.warning
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        MouseArea {
                            id: failedHover
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                        ToolTip.visible: failedHover.containsMouse
                        ToolTip.text: failed.map(p => p.label + ": " + p.error).join("\n")
                    }
                }

                Flow {
                    x: Theme.gutter
                    width: parent.width - 2 * Theme.gutter
                    spacing: 8
                    Repeater {
                        model: page.searchTask && page.searchTask.ok ? page.searchTask.result.providers.filter(p => p.verifyUrl) : []
                        ScButton {
                            required property var modelData
                            compact: true
                            iconName: "globe"
                            text: qsTr("Pass %1's browser check").arg(modelData.label)
                            onClicked: Router.openSiteCheck(modelData.verifyUrl)
                        }
                    }
                }

                CoverGrid {
                    items: page.searchTask && page.searchTask.ok ? page.searchTask.result.comics : []
                    subtitleFor: (it) => it.catalog
                        ? [it.type, it.status, it.lastChapter ? qsTr("Ch. ") + it.lastChapter : ""].filter(s => s).join(" · ")
                        : it.sourceLabel + (it.type ? " · " + it.type : "")
                    onOpened: (it, cover) => it.catalog ? readFrom.start(it) : Router.openSeries(it, cover)
                }

                StateView {
                    width: parent.width
                    task: page.searchTask
                    empty: page.searchTask && page.searchTask.ok && page.searchTask.result.comics.length === 0
                    emptyTitle: page.browsingLocal ? qsTr("No comics on this device yet") : qsTr("No comics found")
                    emptyText: page.browsingLocal
                        ? qsTr("Choose the folders that hold your comics in Settings → Sources. Folders of images, CBZ, CBR and PDF files all work.")
                        : qsTr("Try a shorter title or another source. You can also paste a series link.")
                    emptyIcon: page.browsingLocal ? "folder-open" : "search"
                    onRetry: page.run(page.query)
                }
                ScButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: page.browsingLocal && page.searchTask && page.searchTask.ok && page.searchTask.result.comics.length === 0
                    text: qsTr("Choose folders")
                    iconName: "folder-open"
                    onClicked: Router.openSettings("sources")
                }
            }

            // ---------------------------------------------------- idle
            Column {
                visible: !page.searching
                width: parent.width
                spacing: 34

                RecentSearches {
                    id: recent
                    key: "recentComicSearches"
                    onPicked: (text) => page.search(text)
                }

                Shelf {
                    title: qsTr("Continue reading")
                    items: page.history
                    subtitleFor: (it) => it.unitTitle || ""
                    progressFor: (it) => it.completed ? 1 : (it.percent || 0)
                    onOpened: (it, cover) => Router.resume(it)
                }

                Column {
                    width: parent.width
                    spacing: 16
                    Text {
                        x: Theme.gutter
                        text: qsTr("Latest chapters")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.textXl
                        font.weight: Font.DemiBold
                    }
                    CoverGrid {
                        items: page.latestTask && page.latestTask.ok ? page.latestTask.result.updates : []
                        subtitleFor: (it) => (it.chapterTitle || "") + (it.publishedAt ? " · " + App.relativeTime(it.publishedAt) : "")
                        onOpened: (it, cover) => Router.openSeries(it, cover)
                    }
                    StateView {
                        width: parent.width
                        task: page.latestTask
                        onRetry: page.latestTask = Comics.latest(page.latestLimit)
                    }
                    ScButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: page.latestTask && page.latestTask.ok && page.latestLimit < 120
                        text: qsTr("Show more")
                        onClicked: {
                            page.latestLimit += 36
                            page.latestTask = Comics.latest(page.latestLimit)
                        }
                    }
                }
            }
        }
    }

    BackToTop { flick: flick }
}
