import QtQuick
import ScanCore

// Large page title with an optional subtitle and trailing controls. On
// phones the controls go below the title, which would otherwise be squeezed
// to nothing beside them.
Item {
    id: header
    property string title
    property string subtitle
    property bool showBack: false
    default property alias trailing: trailingRow.data

    readonly property bool stacked: Theme.compact && trailingRow.width > 0
    width: parent ? parent.width : 800
    implicitHeight: column.implicitHeight + 28 + (stacked ? trailingRow.height + 14 : 0)

    IconButton {
        id: back
        visible: header.showBack
        x: Theme.gutter - 8
        y: 26
        iconName: "arrow-left"
        tip: qsTr("Back")
        onClicked: Router.back()
    }

    Column {
        id: column
        x: header.showBack ? Theme.gutter + 34 : Theme.gutter
        y: 24
        width: parent.width - x - Theme.gutter - (header.stacked ? 0 : trailingRow.width + 12)
        spacing: 4
        Text {
            width: parent.width
            text: header.title
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.text3xl
            font.weight: Font.Bold
            font.letterSpacing: -0.6
            elide: Text.ElideRight
        }
        Text {
            visible: text.length > 0
            width: parent.width
            text: header.subtitle
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
            elide: Text.ElideRight
        }
    }

    Row {
        id: trailingRow
        x: header.stacked ? Theme.gutter : parent.width - width - Theme.gutter
        y: header.stacked ? column.y + column.height + 14 : 30
        spacing: 8
    }
}
