import QtQuick
import QtQuick.Controls.Basic
import Scancore

// Round icon button with hover wash, press squish and an optional tooltip.
AbstractButton {
    id: control
    property string iconName
    property int iconSize: 18
    property color iconColor: active ? Theme.text : (hovered ? Theme.text : Theme.textMuted)
    property bool active: false
    property string tip
    property bool filled: false
    property int diameter: 34

    implicitWidth: diameter
    implicitHeight: diameter
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.35

    background: Rectangle {
        radius: control.diameter / 2
        color: control.filled ? Theme.surface2
             : control.active ? Theme.alpha(Theme.text, 0.10)
             : control.hovered ? Theme.alpha(Theme.text, 0.07) : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Motion.fast } }
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.iconColor
            scale: control.pressed ? 0.86 : 1
            Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutBack } }
        }
    }

    // Made only while a pointer rests on it: the attached ToolTip built a
    // popup for every button, which a chapter list makes by the hundred.
    Loader {
        active: control.tip.length > 0 && control.hovered
        sourceComponent: ToolTip {
            parent: control
            visible: true
            delay: 600
            text: control.tip
        }
    }
}
