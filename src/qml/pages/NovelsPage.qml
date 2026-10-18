import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Novel discovery (Royal Road shelves) and search across novel sources.
Item {
    id: page
    property string query: ""
    property string source: App.settings.novelSource || "all"
    property var searchTask: null
    property var discoverTask: Novels.discover()
    property var historyTask: Library.history("novel", 16)
    property var libraryTask: Library.entries("novel")
    // Books on this device are listed without a search.
    readonly property bool browsingLocal: source === "local" && query.length === 0
    readonly property bool searching: query.length > 0 || source === "local"
    Component.onCompleted: if (source === "local") run("")
    readonly property var allSources: { Extensions.novelPackInstalled; return Novels.sources() }
    readonly property var sources: allSources.filter(s => s.enabled && s.searchable && !s.extended)
    readonly property var moreSources: allSources.filter(s => s.enabled && s.searchable && s.extended)
        .sort((a, b) => a.label.localeCompare(b.label))

    Connections {
        target: Library
        function onChanged() { refresh.restart() }
    }
    Timer {
        id: refresh
        interval: 500
        onTriggered: {
            page.historyTask = Library.history("novel", 16)
            page.libraryTask = Library.entries("novel")
        }
    }
    property var history: []
    property var saved: []
    Connections {
        target: page.historyTask
        function onFinished(result) { page.history = Router.keep(page.history, result) }
    }
    Connections {
        target: page.libraryTask
        function onFinished(result) { page.saved = Router.keep(page.saved, result) }
    }

    function search(text) {
        field.text = text
        run(text)
    }

    function run(text) {
        query = text.trim()
        if (query.length > 0) recent.remember(query)
        searchTask = query.length > 0 || source === "local" ? Novels.search(query, source) : null
        flick.scrollTo(0)
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: 26

            PageHeader {
                title: qsTr("Novels")
                subtitle: qsTr("Web novels and light novels from ") + page.sources.length + qsTr(" sources")
            }

            SearchField {
                id: field
                x: Theme.gutter
                width: Math.min(parent.width - 2 * Theme.gutter, 760)
                large: true
                placeholderText: qsTr("Search by title, or paste a book link")
                onSubmitted: (text) => page.run(text)
                onTextChanged: if (text.length === 0 && page.query.length > 0) page.run("")
            }

            ChipRow {
                Select {
                    visible: page.moreSources.length > 0
                    implicitWidth: 220
                    model: [{ value: "", label: qsTr("More sources (") + page.moreSources.length + ")" }]
                        .concat(page.moreSources.map(x => ({ value: x.key, label: x.label + (x.language && x.language !== "en" ? " · " + x.language : "") })))
                    value: page.moreSources.some(x => x.key === page.source) ? page.source : ""
                    onPicked: (v) => {
                        if (!v) return
                        page.source = v
                        if (page.searching) page.run(page.query)
                    }
                }
                Chip {
                    text: qsTr("All sources")
                    selected: page.source === "all"
                    onClicked: {
                        page.source = "all"
                        App.setSetting("novelSource", "all")
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
                            App.setSetting("novelSource", modelData.key)
                            if (page.searching || modelData.key === "local") page.run(page.query)
                        }
                    }
                }
            }

            Column {
                visible: page.searching
                width: parent.width
                spacing: 18
                Text {
                    x: Theme.gutter
                    visible: page.searchTask && page.searchTask.ok
                    text: !page.searchTask || !page.searchTask.ok ? ""
                        : page.browsingLocal ? page.searchTask.result.books.length + qsTr(" on this device")
                        : page.searchTask.result.books.length + qsTr(" results for “") + page.query + "”"
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
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
                    items: page.searchTask && page.searchTask.ok ? page.searchTask.result.books : []
                    subtitleFor: (it) => it.sourceLabel + (it.author ? " · " + it.author : "")
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }
                StateView {
                    width: parent.width
                    task: page.searchTask
                    empty: page.searchTask && page.searchTask.ok && page.searchTask.result.books.length === 0
                    emptyTitle: page.browsingLocal ? qsTr("No books on this device yet") : qsTr("No novels found")
                    emptyText: page.browsingLocal
                        ? qsTr("Choose the folders that hold your EPUB books in Settings → Sources.")
                        : qsTr("Try fewer words, another source, or paste the book's link.")
                    emptyIcon: page.browsingLocal ? "folder-open" : "search"
                    onRetry: page.run(page.query)
                }
                ScButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: page.browsingLocal && page.searchTask && page.searchTask.ok && page.searchTask.result.books.length === 0
                    text: qsTr("Choose folders")
                    iconName: "folder-open"
                    onClicked: Router.openSettings("sources")
                }
            }

            Column {
                visible: !page.searching
                width: parent.width
                spacing: 36

                RecentSearches {
                    id: recent
                    key: "recentNovelSearches"
                    onPicked: (text) => page.search(text)
                }

                Shelf {
                    title: qsTr("Continue reading")
                    items: page.history
                    subtitleFor: (it) => it.unitTitle || ""
                    progressFor: (it) => it.completed ? 1 : (it.percent || 0)
                    onOpened: (it, cover) => Router.resume(it)
                }
                Shelf {
                    title: qsTr("Saved novels")
                    items: page.saved
                    badgeFor: (it) => it.newUnits > 0 ? "+" + it.newUnits : ""
                    subtitleFor: (it) => it.author || ""
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }
                Repeater {
                    model: page.discoverTask && page.discoverTask.ok ? page.discoverTask.result.shelves : []
                    delegate: Shelf {
                        required property var modelData
                        title: modelData.title
                        subtitle: modelData.source
                        items: modelData.books
                        subtitleFor: (it) => it.genres && it.genres.length ? it.genres.slice(0, 2).join(", ") : (it.status || "")
                        badgeFor: (it) => it.rating ? "★ " + Number(it.rating).toFixed(1) : ""
                        onOpened: (it, cover) => Router.openSeries(it, cover)
                    }
                }
                Shelf {
                    visible: page.discoverTask && !page.discoverTask.ok
                    title: qsTr("Discover")
                    loading: page.discoverTask ? page.discoverTask.loading : false
                    error: page.discoverTask && !page.discoverTask.loading ? page.discoverTask.error : ""
                    onRetry: page.discoverTask = Novels.discover()
                }
            }
        }
    }

    BackToTop { flick: flick }
}
