import QtQuick
import ScanCore

// Stacked notifications in the bottom-right corner.
Item {
    id: root
    anchors.fill: parent

    function show(message, kind) {
        toasts.append({ message: message, kind: kind || "info", stamp: Date.now() })
        if (toasts.count > 4) toasts.remove(0)
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
            }
        }
        move: Transition {
            NumberAnimation { properties: "y"; duration: Motion.normal; easing.type: Easing.OutCubic }
        }

        Repeater {
            model: toasts
            delegate: Rectangle {
                id: toast
                required property string message
                required property string kind
                required property int index
                width: root.toastWidth
                height: label.implicitHeight + 28
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.borderStrong

                Rectangle {
                    width: 3
                    height: parent.height - 24
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 2
                    color: toast.kind === "error" ? Theme.danger : toast.kind === "success" ? Theme.success : Theme.accent
                }
                Text {
                    id: label
                    anchors.left: parent.left
                    anchors.leftMargin: 26
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
                    onClicked: toasts.remove(toast.index)
                }
                Timer {
                    interval: toast.kind === "error" ? 7000 : 4200
                    running: true
                    onTriggered: fade.start()
                }
                NumberAnimation {
                    id: fade
                    target: toast
                    property: "opacity"
                    to: 0
                    duration: Motion.normal
                    onFinished: if (toast.index >= 0 && toast.index < toasts.count) toasts.remove(toast.index)
                }
            }
        }
    }
}
