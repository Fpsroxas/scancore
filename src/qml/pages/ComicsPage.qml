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
    readonly property bool searching: query.length > 0

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
        searchTask = query.length > 0 ? Comics.search(query, source) : null
        flick.scrollTo(0)
    }

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
                title: "Comics"
                subtitle: page.sources.length + " sources · manhwa, manga and webtoons"
            }

            SearchField {
                id: field
                x: Theme.gutter
                width: Math.min(parent.width - 2 * Theme.gutter, 760)
                large: true
                placeholderText: "Search titles across every source"
                onSubmitted: (text) => page.run(text)
                onTextChanged: if (text.length === 0 && page.query.length > 0) page.run("")
            }

            ChipRow {
                Chip {
                    text: "All sources"
                    selected: page.source === "all"
                    onClicked: {
                        page.source = "all"
                        App.setSetting("comicSource", "all")
                        if (page.searching) page.run(page.query)
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
                            if (page.searching) page.run(page.query)
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
                        text: page.searchTask && page.searchTask.ok
                              ? page.searchTask.result.comics.length + " results for “" + page.query + "”" : ""
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
                        text: failed.map(p => p.label).join(", ") + (failed.length === 1 ? " is" : " are") + " unavailable"
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
                            text: "Pass " + modelData.label + "'s browser check"
                            onClicked: Router.openSiteCheck(modelData.verifyUrl)
                        }
                    }
                }

                CoverGrid {
                    items: page.searchTask && page.searchTask.ok ? page.searchTask.result.comics : []
                    subtitleFor: (it) => it.sourceLabel + (it.type ? " · " + it.type : "")
                    onOpened: (it, cover) => Router.openSeries(it, cover)
                }

                StateView {
                    width: parent.width
                    task: page.searchTask
                    empty: page.searchTask && page.searchTask.ok && page.searchTask.result.comics.length === 0
                    emptyTitle: "No comics found"
                    emptyText: "Try a shorter title or another source. You can also paste a series link."
                    emptyIcon: "search"
                    onRetry: page.run(page.query)
                }
            }

            // ---------------------------------------------------- idle
            Column {
                visible: !page.searching
                width: parent.width
                spacing: 34

                Shelf {
                    title: "Continue reading"
                    items: page.historyTask && page.historyTask.ok ? page.historyTask.result : []
                    subtitleFor: (it) => it.unitTitle || ""
                    progressFor: (it) => it.completed ? 1 : (it.percent || 0)
                    onOpened: (it, cover) => Router.resume(it)
                }

                Column {
                    width: parent.width
                    spacing: 16
                    Text {
                        x: Theme.gutter
                        text: "Latest chapters"
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
                        text: "Show more"
                        onClicked: {
                            page.latestLimit += 36
                            page.latestTask = Comics.latest(page.latestLimit)
                        }
                    }
                }
            }
        }
    }
}
