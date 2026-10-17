import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Everything read or watched recently, grouped by day.
Item {
    id: page
    property string media: ""
    property var task: Library.history("", 300)

    Connections {
        target: Library
        function onChanged() { refresh.restart() }
    }
    Timer { id: refresh; interval: 300; onTriggered: page.task = Library.history("", 300) }

    function dayLabel(seconds) {
        const d = new Date(seconds * 1000)
        const today = new Date()
        today.setHours(0, 0, 0, 0)
        const diff = Math.floor((today - new Date(d.getFullYear(), d.getMonth(), d.getDate())) / 86400000)
        return diff <= 0 ? "Today" : diff === 1 ? "Yesterday" : diff < 7 ? Qt.formatDate(d, "dddd") : Qt.formatDate(d, "d MMMM yyyy")
    }

    readonly property var rows: {
        const all = (task && task.ok ? task.result : []).filter(e => !media || e.media === media)
        const out = []
        let last = ""
        for (const e of all) {
            const label = dayLabel(e.progressAt)
            if (label !== last) {
                out.push({ header: label })
                last = label
            }
            out.push({ entry: e })
        }
        return out
    }

    ListView {
        id: list
        anchors.fill: parent
        model: page.rows
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        bottomMargin: 40
        ScrollBar.vertical: ScScrollBar {}

        header: Column {
            width: list.width
            spacing: 18
            PageHeader {
                title: "History"
                subtitle: "What you read and watched, newest first"
                ScButton {
                    text: "Clear history"
                    iconName: "trash"
                    variant: "danger"
                    enabled: page.rows.length > 0
                    onClicked: confirm.open()
                }
            }
            Segmented {
                x: Theme.gutter
                options: [{ value: "", label: "All" }, { value: "comic", label: "Comics", icon: "book-open" },
                          { value: "novel", label: "Novels", icon: "book-text" }, { value: "anime", label: "Anime", icon: "tv" }]
                value: page.media
                onPicked: (v) => page.media = v
            }
            StateView {
                width: list.width
                height: visible ? 300 : 0
                task: page.task
                empty: page.task && page.task.ok && page.rows.length === 0
                emptyTitle: "Nothing read yet"
                emptyText: "Chapters and episodes you open show up here, with your place saved."
                emptyIcon: "history"
                onRetry: page.task = Library.history("", 300)
            }
            Item { width: 1; height: 4 }
        }

        delegate: Loader {
            required property var modelData
            width: list.width
            sourceComponent: modelData.header ? headerRow : entryRow
            property var row: modelData
        }

        Component {
            id: headerRow
            Item {
                height: 44
                Text {
                    x: Theme.gutter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    text: parent.parent ? parent.parent.row.header : ""
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs
                    font.weight: Font.Bold
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 1.2
                }
            }
        }

        Component {
            id: entryRow
            AbstractButton {
                id: item
                readonly property var e: parent ? parent.row.entry : ({})
                height: 76
                hoverEnabled: true
                onClicked: Router.resume(e)
                background: Rectangle {
                    x: Theme.gutter - 12
                    width: item.width - 2 * Theme.gutter + 24
                    radius: Theme.radiusLg
                    color: item.hovered ? Theme.surface2 : "transparent"
                    Behavior on color { ColorAnimation { duration: Motion.fast } }
                }
                contentItem: Item {
                    Cover {
                        id: cover
                        x: Theme.gutter
                        anchors.verticalCenter: parent.verticalCenter
                        width: 42
                        height: 60
                        radius: 7
                        source: item.e.cover || ""
                        title: item.e.title || ""
                    }
                    Column {
                        anchors.left: cover.right
                        anchors.leftMargin: 16
                        anchors.right: actions.left
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4
                        Text {
                            width: parent.width
                            text: item.e.title || ""
                            color: Theme.text
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textMd
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: (item.e.media === "anime" ? "Episode " + (item.e.unitNumber || item.e.unitKey) : (item.e.unitTitle || ""))
                                + " · " + (item.e.completed ? "finished" : Math.round((item.e.percent || 0) * 100) + "%")
                                + " · " + App.relativeTime(item.e.progressAt)
                            color: Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textSm
                            elide: Text.ElideRight
                        }
                    }
                    Row {
                        id: actions
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.gutter - 6
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: item.hovered || Theme.touch ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: Motion.fast } }
                        IconButton { iconName: "info"; tip: "Details"; onClicked: Router.openSeries(item.e, cover) }
                        IconButton { iconName: "x"; tip: "Remove from history"; onClicked: Library.forget(item.e.id) }
                    }
                }
            }
        }
    }

    Dialog {
        id: confirm
        anchors.centerIn: parent
        modal: true
        title: ""
        width: Math.min(400, page.width - 32)
        padding: 24
        background: Rectangle { radius: Theme.radiusLg; color: Theme.surface; border.width: 1; border.color: Theme.borderStrong }
        contentItem: Column {
            spacing: 16
            Text { text: "Clear " + (page.media ? page.media + " " : "") + "history?"; color: Theme.text; font.family: Theme.fontDisplay; font.pixelSize: Theme.textLg; font.weight: Font.DemiBold }
            Text {
                width: confirm.availableWidth
                wrapMode: Text.Wrap
                text: "Your place in every series will be forgotten. Your library and downloads are kept."
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
            }
            Row {
                anchors.right: parent.right
                spacing: 8
                ScButton { text: "Cancel"; onClicked: confirm.close() }
                ScButton { text: "Clear"; variant: "danger"; onClicked: { Library.clearHistory(page.media); confirm.close() } }
            }
        }
    }
}
