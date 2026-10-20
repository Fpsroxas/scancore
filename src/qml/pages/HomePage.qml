import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Everything at a glance: continue where you left off, library updates and
// what is new across comics, novels and anime.
Item {
    id: page

    property var historyTask: Library.continueItems("", 24)
    property var libraryTask: Library.entries("")
    property var latestTask: Comics.latest(30)
    property var animeTask: Anime.home()
    property var novelsTask: Novels.discover()

    // The last results, kept while a refresh runs.
    property var history: []
    property var library: []
    readonly property var updates: library.filter(e => e.newUnits > 0)
    Connections {
        target: page.historyTask
        function onFinished(result) { page.history = Router.keep(page.history, result) }
    }
    Connections {
        target: page.libraryTask
        function onFinished(result) { page.library = Router.keep(page.library, result) }
    }

    Connections {
        target: Library
        function onChanged() { refresh.restart() }
    }
    Timer {
        id: refresh
        interval: 400
        onTriggered: {
            page.historyTask = Library.continueItems("", 24)
            page.libraryTask = Library.entries("")
        }
    }

    // Edit page: arrange the sections (PageLayout).
    property bool editing: false
    PageArranger {
        id: layout
        page: "home"
        column: column
        editing: page.editing
        defaults: ["greeting", "resume", "continue", "updates", "comics", "anime", "novels"]
    }

    function greeting() {
        const h = new Date().getHours()
        return h < 5 ? qsTr("Up late") : h < 12 ? qsTr("Good morning") : h < 18 ? qsTr("Good afternoon") : qsTr("Good evening")
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60

        Column {
            id: column
            width: parent.width
            spacing: Theme.sectionGap

            // Edit page, at the top right whatever comes first.
            Item {
                width: parent.width
                height: editButton.height
                IconButton {
                    id: editButton
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.gutter - 6
                    y: 12
                    iconName: "layout-dashboard"
                    tip: qsTr("Edit page")
                    active: page.editing
                    onClicked: page.editing = !page.editing
                }
            }

            PageSection {
                arranger: layout
                key: "greeting"
                label: qsTr("Greeting")
            Item {
                width: parent.width
                height: Theme.compact ? 70 : 110
                Column {
                    id: heading
                    x: Theme.gutter
                    anchors.bottom: parent.bottom
                    spacing: 6
                    Text {
                        text: Qt.formatDate(new Date(), "dddd, d MMMM")
                        color: Theme.textFaint
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        font.weight: Font.Medium
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 1.2
                    }
                    Text {
                        text: page.greeting()
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.compact ? 32 : 42
                        font.weight: Font.Bold
                        font.letterSpacing: -1
                    }
                }
                // Only when it fits beside the greeting (not on landscape phones).
                Row {
                    id: quickLinks
                    visible: !Theme.compact && heading.width + width + 3 * Theme.gutter <= parent.width
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.gutter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    spacing: 10
                    ScButton {
                        text: qsTr("Comics")
                        iconName: "book-open"
                        onClicked: Router.go("comics")
                    }
                    ScButton {
                        text: qsTr("Novels")
                        iconName: "book-text"
                        onClicked: Router.go("novels")
                    }
                    ScButton {
                        text: qsTr("Anime")
                        iconName: "tv"
                        onClicked: Router.go("anime")
                    }
                }
            }
            }

            // The latest of them, large.
            PageSection {
                arranger: layout
                key: "resume"
                label: qsTr("Last read or watched")
                shown: page.history.length > 0
                Loader {
                    width: parent.width
                    active: page.history.length > 0
                    visible: active
                    sourceComponent: ResumeCard { entry: page.history[0] }
                }
            }

            PageSection {
                arranger: layout
                key: "continue"
                label: qsTr("Continue")
                Shelf {
                    title: qsTr("Continue")
                    items: page.history.slice(1)
                    loading: page.historyTask ? page.historyTask.loading && page.history.length === 0 : false
                    subtitleFor: (it) => it.media === "anime" ? qsTr("Episode ") + (it.unitNumber || it.unitKey) : (it.unitTitle || "")
                    progressFor: (it) => it.completed ? 1 : (it.percent || 0)
                    onOpened: (it, cover) => Router.resume(it)
                }
            }

            PageSection {
                arranger: layout
                key: "updates"
                label: qsTr("New in your library")
                Shelf {
                    title: qsTr("New in your library")
                    items: page.updates
                    badgeFor: (it) => "+" + it.newUnits
                    subtitleFor: (it) => it.latestTitle || ""
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }
            }

            PageSection {
                arranger: layout
                key: "comics"
                label: qsTr("Latest comic chapters")
                Shelf {
                    title: qsTr("Latest comic chapters")
                    actionText: qsTr("More")
                    onAction: Router.go("comics")
                    items: page.latestTask && page.latestTask.ok ? page.latestTask.result.updates : []
                    loading: page.latestTask ? page.latestTask.loading : false
                    error: page.latestTask && !page.latestTask.loading ? page.latestTask.error : ""
                    onRetry: page.latestTask = Comics.latest(30)
                    subtitleFor: (it) => (it.chapterTitle || "") + (it.publishedAt ? " · " + App.relativeTime(it.publishedAt) : "")
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }
            }

            PageSection {
                arranger: layout
                key: "anime"
                label: qsTr("Trending anime")
                Shelf {
                    title: qsTr("Trending anime")
                    actionText: qsTr("Browse")
                    onAction: Router.go("anime")
                    items: page.animeTask && page.animeTask.ok ? page.animeTask.result.trending : []
                    loading: page.animeTask ? page.animeTask.loading : false
                    error: page.animeTask && !page.animeTask.loading ? page.animeTask.error : ""
                    onRetry: page.animeTask = Anime.home()
                    subtitleFor: (it) => [it.format, it.year].filter(Boolean).join(" · ")
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }
            }

            PageSection {
                arranger: layout
                key: "novels"
                label: qsTr("Novels")
                Column {
                    readonly property bool hasContent: page.novelsTask !== null
                    width: parent.width
                    spacing: Theme.sectionGap
                    Repeater {
                        model: page.novelsTask && page.novelsTask.ok ? page.novelsTask.result.shelves.slice(0, 2) : []
                        delegate: Shelf {
                            required property var modelData
                            title: modelData.title + qsTr(" novels")
                            subtitle: modelData.source
                            items: modelData.books
                            subtitleFor: (it) => it.author || it.genres.slice(0, 2).join(", ")
                            onOpened: (it, cover) => Router.openSeries(it, cover)
                        }
                    }
                    Shelf {
                        visible: page.novelsTask && (page.novelsTask.loading || page.novelsTask.error.length > 0)
                        title: qsTr("Novels")
                        loading: page.novelsTask ? page.novelsTask.loading : false
                        error: page.novelsTask && !page.novelsTask.loading ? page.novelsTask.error : ""
                        onRetry: page.novelsTask = Novels.discover()
                    }
                }
            }
        }
    }

    BackToTop { flick: flick }
    EditPageBar {
        editing: page.editing
        arrangers: [layout]
        onDone: page.editing = false
    }
}
