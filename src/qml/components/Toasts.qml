import QtQuick
import ScanCore

// Stacked notifications in the bottom-right corner. Each slides in, shows how
// long it will stay with a thin bar, pauses while the pointer rests on it,
// and can be swiped (or dragged) sideways to dismiss.
Item {
    id: root
    anchors.fill: parent

    function show(message, kind) {
        toasts.append({ message: message, kind: kind || "info", stamp: Date.now() })
        if (toasts.count > 4) toasts.remove(0)
    }

    function removeStamp(stamp) {
        for (let i = 0; i < toasts.count; ++i)
            if (toasts.get(i).stamp === stamp) {
                toasts.remove(i)
                return
            }
    }

    ListModel { id: toasts }

    // Phones are narrower than a full-size toast plus its margin.
    readonly property int margin: Theme.compact ? 16 : 24
    readonly property real toastWidth: Math.min(360, width - 2 * margin)

    Column {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.margin
        anchors.bottomMargin: root.margin + root.SafeArea.margins.bottom
        spacing: 10
        width: root.toastWidth

        add: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal }
                NumberAnimation { property: "x"; from: 40; duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
                NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Motion.slow; easing.type: Easing.OutCubic }
            }
        }
        move: Transition {
            NumberAnimation { properties: "y"; duration: Motion.normal; easing.type: Easing.OutCubic }
        }

        Repeater {
            model: toasts
            delegate: Item {
                id: toast
                required property string message
                required property string kind
                required property real stamp
                readonly property int lifetime: kind === "error" ? 7000 : 4200
                readonly property color tone: kind === "error" ? Theme.danger : kind === "success" ? Theme.success : Theme.accent
                property real elapsed: 0
                property bool leaving: false

                width: root.toastWidth
                height: card.height

                function dismiss(direction) {
                    if (leaving) return
                    leaving = true
                    leave.direction = direction || 1
                    leave.start()
                }

                Rectangle {
                    id: card
                    width: parent.width
                    height: Math.max(52, label.implicitHeight + 28)
                    radius: Theme.radiusLg
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.borderStrong
                    clip: true

                    Rectangle {
                        id: badge
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28
                        height: 28
                        radius: 14
                        color: Theme.alpha(toast.tone, 0.15)
                        Icon {
                            anchors.centerIn: parent
                            name: toast.kind === "error" ? "triangle-alert" : toast.kind === "success" ? "check" : "info"
                            size: 15
                            color: toast.tone
                        }
                    }
                    Text {
                        id: label
                        anchors.left: badge.right
                        anchors.leftMargin: 12
                        anchors.right: close.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: toast.message
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        wrapMode: Text.Wrap
                    }
                    IconButton {
                        id: close
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        diameter: 28
                        iconSize: 14
                        iconName: "x"
                        onClicked: toast.dismiss(1)
                    }
                    // Time left.
                    Rectangle {
                        x: 14
                        anchors.bottom: parent.bottom
                        height: 2
                        radius: 1
                        width: (parent.width - 28) * Math.max(0, 1 - toast.elapsed / toast.lifetime)
                        color: Theme.alpha(toast.tone, 0.55)
                        visible: !Motion.reduced
                    }
                }

                HoverHandler { id: hover }
                DragHandler {
                    id: drag
                    target: card
                    yAxis.enabled: false
                    onActiveChanged: {
                        if (active) return
                        if (Math.abs(card.x) > card.width * 0.3) toast.dismiss(card.x > 0 ? 1 : -1)
                        else settle.start()
                    }
                }
                NumberAnimation { id: settle; target: card; property: "x"; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
                opacity: 1 - Math.min(0.6, Math.abs(card.x) / card.width)

                // Counts down while the toast is neither hovered nor held.
                Timer {
                    interval: 50
                    repeat: true
                    running: !toast.leaving && !hover.hovered && !drag.active
                    onTriggered: {
                        toast.elapsed += interval
                        if (toast.elapsed >= toast.lifetime) toast.dismiss(1)
                    }
                }

                SequentialAnimation {
                    id: leave
                    property int direction: 1
                    ParallelAnimation {
                        NumberAnimation { target: card; property: "x"; to: leave.direction * (card.width + 40); duration: Motion.normal; easing.type: Easing.InCubic }
                        NumberAnimation { target: toast; property: "opacity"; to: 0; duration: Motion.normal }
                    }
                    ScriptAction { script: root.removeStamp(toast.stamp) }
                }
            }
        }
    }
}
