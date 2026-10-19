import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Saved comics, novels and anime, each on its own: categories (reading,
// planned, on hold, completed, dropped), favourites, three views (grid,
// compact grid, list) and update checks.
Item {
    id: page
    property string filter: ""
    property var task: Library.entries("", true)
    property var checkTask: null

    readonly property string media: App.settings.libraryMedia || "comic"
    readonly property string category: App.settings.libraryCategory || "all"
    readonly property string view: App.settings.libraryView || "grid"
    readonly property string sort: App.settings.librarySort || "updates"
    readonly property var categories: Router.libraryCategories(media)

    // The last result, kept while a refresh runs.
    property var all: []
    Connections {
        target: page.task
        function onFinished(result) { page.all = Router.keep(page.all, result) }
    }
    readonly property var inMedia: all.filter(e => e.media === media)
    // How many series each chip holds.
    readonly property var counts: {
        const c = { all: inMedia.length, favourites: 0 }
        for (const e of inMedia) {
            const k = e.category || "reading"
            c[k] = (c[k] || 0) + 1
            if (e.favourite) c.favourites += 1
        }
        return c
    }
    readonly property var entries: {
        const f = filter.trim().toLowerCase()
        const out = inMedia.filter(e => (category === "all" || (category === "favourites" ? !!e.favourite : (e.category || "reading") === category))
                                        && (!f || (e.title || "").toLowerCase().includes(f)))
        // "updates" keeps the stored order: new chapters first, then recent.
        if (sort === "recent") out.sort((a, b) => (b.progressAt || 0) - (a.progressAt || 0))
        else if (sort === "added") out.sort((a, b) => (b.addedAt || 0) - (a.addedAt || 0))
        else if (sort === "title") out.sort((a, b) => String(a.title || "").localeCompare(String(b.title || "")))
        return out
    }
    readonly property string categoryLabel: category === "favourites" ? qsTr("Favourites")
        : (categories.find(c => c.value === category) || { label: "" }).label

    function search(text) { filter = text }
    function subtitle(it) {
        return it.unitTitle
            ? (it.media === "anime" ? qsTr("Episode ") + (it.unitNumber || it.unitKey) : it.unitTitle)
            : (it.media === "anime" ? qsTr("Anime") : it.media === "novel" ? qsTr("Novel") : qsTr("Comic"))
    }

    Connections {
        target: Library
        function onChanged() { refresh.restart() }
    }
    Timer { id: refresh; interval: 300; onTriggered: page.task = Library.entries("", true) }
    Connections {
        target: page.checkTask
        function onFinished(result) {
            Router.toast(result.newUnits > 0 ? result.newUnits + qsTr(" new chapters found") : qsTr("Everything is up to date"),
                         result.newUnits > 0 ? "success" : "info")
        }
        function onFailed(error) { Router.toast(error, "error") }
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: 20

            PageHeader {
                title: qsTr("Library")
                subtitle: page.all.length + qsTr(" saved")
                    + (Library.unreadUpdates > 0 ? " · " + Library.unreadUpdates + qsTr(" new chapters") : "")
                ScButton {
                    text: page.checkTask && page.checkTask.loading ? qsTr("Checking…") : qsTr("Check for updates")
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
                    options: [{ value: "comic", label: qsTr("Comics"), icon: "book-open" },
                              { value: "novel", label: qsTr("Novels"), icon: "book-text" }, { value: "anime", label: qsTr("Anime"), icon: "tv" }]
                    value: page.media
                    onPicked: (v) => App.setSetting("libraryMedia", v)
                }
                SearchField {
                    width: Theme.compact ? parent.width : 240
                    placeholderText: qsTr("Filter library")
                    text: page.filter
                    onTextChanged: page.filter = text
                }
                Select {
                    implicitWidth: 196
                    model: [{ value: "updates", label: qsTr("New chapters first") }, { value: "recent", label: page.media === "anime" ? qsTr("Recently watched") : qsTr("Recently read") },
                            { value: "added", label: qsTr("Recently added") }, { value: "title", label: qsTr("Title A–Z") }]
                    value: page.sort
                    onPicked: (v) => App.setSetting("librarySort", v)
                }
                Segmented {
                    options: [{ value: "grid", icon: "layout-grid" }, { value: "compact", icon: "grid-2x2" }, { value: "list", icon: "list" }]
                    value: page.view
                    onPicked: (v) => App.setSetting("libraryView", v)
                }
            }

            ChipRow {
                Chip {
                    text: qsTr("All")
                    detail: String(page.counts.all || 0)
                    selected: page.category === "all"
                    onClicked: App.setSetting("libraryCategory", "all")
                }
                Chip {
                    text: qsTr("Favourites")
                    iconName: "heart"
                    detail: String(page.counts.favourites || 0)
                    selected: page.category === "favourites"
                    onClicked: App.setSetting("libraryCategory", "favourites")
                }
                Repeater {
                    model: page.categories
                    Chip {
                        required property var modelData
                        text: modelData.label
                        detail: String(page.counts[modelData.value] || 0)
                        selected: page.category === modelData.value
                        onClicked: App.setSetting("libraryCategory", modelData.value)
                    }
                }
            }

            CoverGrid {
                visible: page.view !== "list"
                items: page.view !== "list" ? page.entries : []
                minCardWidth: page.view === "compact" ? Math.round((Theme.compact ? 78 : 112) * Theme.coverScale)
                                                      : Math.round((Theme.compact ? 104 : 150) * Theme.coverScale)
                badgeFor: (it) => it.newUnits > 0 ? "+" + it.newUnits : ""
                progressFor: (it) => it.unitKey ? (it.completed ? 1 : (it.percent || 0)) : -1
                subtitleFor: (it) => page.subtitle(it)
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }
            CoverList {
                visible: page.view === "list"
                items: page.view === "list" ? page.entries : []
                badgeFor: (it) => it.newUnits > 0 ? "+" + it.newUnits : ""
                progressFor: (it) => it.unitKey ? (it.completed ? 1 : (it.percent || 0)) : -1
                subtitleFor: (it) => [page.subtitle(it), it.sourceLabel || ""].filter(s => s).join(" · ")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }

            StateView {
                width: parent.width
                task: page.all.length === 0 ? page.task : null
                empty: page.task && page.task.ok && page.entries.length === 0
                emptyTitle: page.filter ? qsTr("Nothing matches")
                    : page.all.length === 0 ? qsTr("Your library is empty")
                    : page.category === "all" ? qsTr("Nothing saved here yet")
                    : qsTr("Nothing in %1").arg(page.categoryLabel)
                emptyText: page.all.length === 0 || page.category === "all"
                    ? qsTr("Open any comic, novel or anime and choose “Add to library” to keep it here. SCANCORE checks saved series for new chapters in the background.")
                    : page.category === "favourites" ? qsTr("Tap the heart on a saved series to keep it here.")
                    : qsTr("Choose a category on a saved series' page to move it here.")
                emptyIcon: page.category === "favourites" ? "heart" : "library"
                onRetry: page.task = Library.entries("", true)
            }
        }
    }

    BackToTop { flick: flick }
}
