import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Opens a title from Comick's catalogue: finds it on the sources (under any
// of its names) and opens it there, or asks which one when several have it.
Dialog {
    id: dialog
    property var item: ({})
    property var task: null
    readonly property var matches: task && task.ok ? task.result.matches : []

    function start(entry) {
        item = entry
        task = Comics.resolve(entry)
        open()
    }

    function show(match) {
        close()
        Router.openSeries({ media: "comic", source: match.source, sourceUrl: match.url, title: match.title,
                            cover: match.cover || dialog.item.cover, description: dialog.item.description || "" }, null)
    }

    Connections {
        target: dialog.task
        function onFinished(result) {
            if (result.matches.length === 1) dialog.show(result.matches[0])
        }
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, (parent ? parent.width : 460) - 32)
    modal: true
    focus: true
    padding: 24
    title: ""

    Overlay.modal: Rectangle { color: Theme.overlay }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring }
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
        spacing: 16

        Row {
            width: parent.width
            spacing: 14
            Cover {
                width: 54
                height: 78
                radius: 8
                source: dialog.item.cover || ""
            }
            Column {
                width: parent.width - 68
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    width: parent.width
                    text: dialog.item.title || ""
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.textLg
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: !dialog.task || dialog.task.loading ? qsTr("Looking for it on your sources…")
                        : !dialog.task.ok ? dialog.task.error
                        : dialog.matches.length === 0 ? qsTr("None of your sources has it yet.")
                        : qsTr("Read it from")
                    color: dialog.task && !dialog.task.loading && !dialog.task.ok ? Theme.danger : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    wrapMode: Text.Wrap
                }
            }
        }

        Spinner {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !!dialog.task && dialog.task.loading
        }

        Column {
            width: parent.width
            spacing: 6
            Repeater {
                model: dialog.matches
                AbstractButton {
                    id: option
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: 54
                    hoverEnabled: true
                    onClicked: dialog.show(modelData)
                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: option.hovered ? Theme.surface3 : Theme.surface2
                        border.width: 1
                        border.color: option.index === 0 ? Theme.alpha(Theme.accent, 0.4) : Theme.border
                    }
                    contentItem: Item {
                        Column {
                            x: 14
                            width: parent.width - 52
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                text: option.modelData.sourceLabel
                                color: Theme.text
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textMd
                                font.weight: Font.DemiBold
                            }
                            Text {
                                width: parent.width
                                text: option.modelData.title
                                color: Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textSm
                                elide: Text.ElideRight
                            }
                        }
                        Icon {
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            name: "chevron-right"
                            size: 16
                            color: Theme.textFaint
                        }
                    }
                }
            }
        }

        Row {
            anchors.right: parent.right
            spacing: 8
            ScButton {
                visible: !!dialog.item.slug
                text: qsTr("Comick")
                iconName: "external-link"
                variant: "ghost"
                onClicked: App.openUrl("https://comick.dev/comic/" + dialog.item.slug)
            }
            ScButton {
                text: qsTr("Close")
                onClicked: dialog.close()
            }
        }
    }
}
