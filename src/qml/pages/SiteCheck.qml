import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Opens a site so the reader can pass its browser check once. SCANCORE then
// uses the cookies the site sets for its own requests to that site.
Rectangle {
    id: page
    property string url
    readonly property string host: url.replace(/^https?:\/\//, "").replace(/\/.*$/, "")
    color: Theme.base
    focus: true
    // A shortcut, because the embedded browser keeps keys once it has focus.
    Shortcut { sequence: "Esc"; onActivated: Router.back() }

    Rectangle {
        id: bar
        width: parent.width
        height: 64
        color: Theme.surface
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        Row {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            IconButton { iconName: "arrow-left"; tip: "Close"; onClicked: Router.back() }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: "Browser check for " + page.host
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Complete any check the site shows. When its normal page appears, press Done."
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                }
            }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            IconButton { iconName: "refresh-cw"; tip: "Reload"; onClicked: if (loader.item) loader.item.reload() }
            ScButton {
                variant: "primary"
                text: "Done"
                iconName: "check"
                onClicked: {
                    Router.toast("Try again now; " + page.host + " should load.", "success")
                    Router.back()
                }
            }
        }
    }

    Loader {
        id: loader
        anchors.top: bar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        active: App.webEngine
        source: "../components/WebCheckView.qml"
        onLoaded: item.startUrl = page.url
    }
    Text {
        anchors.centerIn: parent
        visible: !App.webEngine
        text: "This build has no embedded browser. Open " + page.host + " in your browser instead."
        color: Theme.textMuted
        font.family: Theme.fontSans
        font.pixelSize: Theme.textMd
    }
}
