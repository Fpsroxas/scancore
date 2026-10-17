import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Offline chapters: queue, progress, failures and stored copies.
Item {
    id: page
    property var task: Downloads.items()
    property var sizeText: Downloads.sizeOnDisk()

    Connections {
        target: Downloads
        function onChanged() { refresh.restart() }
    }
    Timer {
        id: refresh
        interval: 250
        onTriggered: page.task = Downloads.items()
    }

    readonly property var items: task && task.ok ? task.result : []
    // Measuring walks the whole downloads folder, so only when what is saved
    // changes, not on every page of a running download.
    readonly property int savedCount: items.filter(i => i.status === "done").length
    onSavedCountChanged: sizeText = Downloads.sizeOnDisk()
    readonly property int failedCount: items.filter(i => i.status === "failed").length

    ListView {
        id: list
        anchors.fill: parent
        model: page.items
        clip: true
        bottomMargin: 40
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScScrollBar {}

        header: Column {
            width: list.width
            spacing: 14
            PageHeader {
                title: "Downloads"
                subtitle: page.items.filter(i => i.status === "done").length + " chapters saved · " + page.sizeText
                Row {
                    spacing: 8
                    ScButton { visible: page.failedCount > 0; text: "Retry failed"; iconName: "refresh-cw"; onClicked: Downloads.retryFailed() }
                    ScButton { visible: page.failedCount > 0; text: "Clear failed"; variant: "ghost"; onClicked: Downloads.clearFinished() }
                    ScButton { visible: App.platform !== "android"; text: "Open folder"; iconName: "folder-open"; onClicked: App.openFolder(Downloads.folder()) }
                }
            }
            StateView {
                width: list.width
                height: visible ? 320 : 0
                task: page.task
                empty: page.task && page.task.ok && page.items.length === 0
                emptyTitle: "No downloads yet"
                emptyText: "On a comic or novel page, use the download button on a chapter, or select several at once. Downloaded chapters open without a connection."
                emptyIcon: "download"
            }
        }

        delegate: Item {
            id: row
            required property var modelData
            width: list.width
            height: 74

            Rectangle {
                x: Theme.gutter - 12
                width: parent.width - 2 * Theme.gutter + 24
                height: parent.height - 6
                radius: Theme.radiusLg
                color: hover.hovered ? Theme.surface2 : "transparent"
                Behavior on color { ColorAnimation { duration: Motion.fast } }
            }
            HoverHandler { id: hover }

            Cover {
                id: cover
                x: Theme.gutter
                anchors.verticalCenter: parent.verticalCenter
                width: 40
                height: 56
                radius: 7
                source: row.modelData.cover || ""
            }
            Column {
                id: details
                anchors.left: cover.right
                anchors.leftMargin: 16
                anchors.right: actions.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Text {
                    width: parent.width
                    text: (row.modelData.seriesTitle || "") + " · " + (row.modelData.unitTitle || "")
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Row {
                    id: statusRow
                    width: parent.width
                    spacing: 10
                    Rectangle {
                        id: bar
                        visible: row.modelData.status === "running" || row.modelData.status === "queued"
                        width: Math.min(160, details.width * 0.45)
                        height: 4
                        radius: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.surface3
                        Rectangle {
                            width: parent.width * (row.modelData.total > 0 ? row.modelData.done / row.modelData.total : 0)
                            height: parent.height
                            radius: 2
                            color: Theme.accent
                            Behavior on width { NumberAnimation { duration: Motion.normal } }
                        }
                    }
                    Text {
                        text: row.modelData.status === "running" ? (row.modelData.total ? row.modelData.done + " of " + row.modelData.total + " pages" : "Starting…")
                            : row.modelData.status === "queued" ? "Waiting"
                            : row.modelData.status === "failed" ? (row.modelData.error || "Failed")
                            : "Saved " + App.relativeTime(row.modelData.createdAt)
                        color: row.modelData.status === "failed" ? Theme.danger : Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, statusRow.width - (bar.visible ? bar.width + statusRow.spacing : 0))
                    }
                }
            }
            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: Theme.gutter - 6
                anchors.verticalCenter: parent.verticalCenter
                Spinner { visible: row.modelData.status === "running"; size: 16; anchors.verticalCenter: parent.verticalCenter }
                IconButton { visible: row.modelData.status === "failed"; iconName: "refresh-cw"; tip: "Retry"; onClicked: Downloads.retry(row.modelData.id) }
                IconButton {
                    visible: row.modelData.status === "done"
                    iconName: "book-open"
                    tip: "Open series"
                    onClicked: Router.openSeries({ media: row.modelData.media, sourceUrl: row.modelData.sourceUrl, title: row.modelData.seriesTitle, cover: row.modelData.cover, source: row.modelData.source }, cover)
                }
                IconButton { visible: row.modelData.status !== "running"; iconName: "trash"; tip: "Delete"; onClicked: Downloads.remove(row.modelData.id) }
            }
        }
    }
}
