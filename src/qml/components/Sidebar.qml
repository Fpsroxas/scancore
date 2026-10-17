import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Collapsible navigation rail. The selection indicator slides between
// items; collapsing animates the width and fades the labels.
Rectangle {
    id: sidebar
    property string current: "home"
    property bool collapsed: App.settings.sidebarCollapsed === true
    signal navigate(string section)
    signal openPalette()

    readonly property var sections: [
        { key: "home", label: "Home", icon: "house" },
        { key: "comics", label: "Comics", icon: "book-open" },
        { key: "novels", label: "Novels", icon: "book-text" },
        { key: "anime", label: "Anime", icon: "tv" },
        { key: "divider" },
        { key: "library", label: "Library", icon: "library" },
        { key: "history", label: "History", icon: "history" },
        { key: "downloads", label: "Downloads", icon: "download" }
    ]

    implicitWidth: collapsed ? 72 : 228
    width: implicitWidth
    color: Theme.sidebar
    Behavior on width { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    // Brand
    Item {
        id: brand
        width: parent.width
        height: 72
        Icon {
            id: logo
            x: sidebar.collapsed ? 18 : 22
            anchors.verticalCenter: parent.verticalCenter
            name: "scancore-mark"
            size: 36
            height: 18
            sourceSize: Qt.size(72 * Screen.devicePixelRatio, 36 * Screen.devicePixelRatio)
            fillMode: Image.PreserveAspectFit
            color: Theme.text
            Behavior on x { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
        }
        Text {
            anchors.left: logo.right
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: "SCANCORE"
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 15
            font.weight: Font.DemiBold
            font.letterSpacing: 3.2
            opacity: sidebar.collapsed ? 0 : 1
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: Motion.fast } }
        }
    }

    // Search / command palette button
    AbstractButton {
        id: searchButton
        x: 12
        y: brand.height
        width: parent.width - 24
        height: 38
        hoverEnabled: true
        onClicked: sidebar.openPalette()
        background: Rectangle {
            radius: Theme.radiusMd
            color: searchButton.hovered ? Theme.surface3 : Theme.surface2
            border.width: 1
            border.color: Theme.border
            Behavior on color { ColorAnimation { duration: Motion.fast } }
        }
        contentItem: Item {
            Icon {
                x: sidebar.collapsed ? (parent.width - width) / 2 : 12
                anchors.verticalCenter: parent.verticalCenter
                name: "search"
                size: 16
                color: Theme.textMuted
                Behavior on x { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
            }
            Text {
                x: 38
                anchors.verticalCenter: parent.verticalCenter
                text: "Search everything"
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                opacity: sidebar.collapsed ? 0 : 1
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Motion.fast } }
            }
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: kbd.implicitWidth + 10
                height: 20
                radius: 5
                color: Theme.surface3
                opacity: sidebar.collapsed ? 0 : 1
                visible: opacity > 0
                Text {
                    id: kbd
                    anchors.centerIn: parent
                    text: App.platform === "macos" ? "⌘K" : "Ctrl K"
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: 11
                }
            }
        }
        ToolTip.visible: sidebar.collapsed && hovered
        ToolTip.text: "Search (Ctrl+K)"
    }

    Item {
        id: navArea
        y: searchButton.y + searchButton.height + 18
        width: parent.width
        height: navColumn.height

        // Sliding selection indicator.
        Rectangle {
            id: indicator
            readonly property Item target: {
                for (let i = 0; i < repeater.count; ++i) {
                    const it = repeater.itemAt(i)
                    if (it && it.key === sidebar.current) return it
                }
                return null
            }
            visible: target !== null
            x: 12
            width: parent.width - 24
            height: 40
            y: target ? target.y : 0
            radius: Theme.radiusMd
            color: Theme.alpha(Theme.text, Theme.dark ? 0.08 : 0.07)
            Behavior on y { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
            Rectangle {
                x: -12
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: 18
                radius: 2
                color: Theme.accent
            }
        }

        Column {
            id: navColumn
            width: parent.width
            spacing: 2
            Repeater {
                id: repeater
                model: sidebar.sections
                delegate: Item {
                    required property var modelData
                    readonly property string key: modelData.key
                    width: navColumn.width
                    height: key === "divider" ? 22 : 40

                    Rectangle {
                        visible: parent.key === "divider"
                        anchors.centerIn: parent
                        width: parent.width - 40
                        height: 1
                        color: Theme.border
                    }

                    AbstractButton {
                        id: navButton
                        visible: parent.key !== "divider"
                        x: 12
                        width: parent.width - 24
                        height: 40
                        hoverEnabled: true
                        onClicked: sidebar.navigate(modelData.key)
                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: navButton.hovered && sidebar.current !== modelData.key ? Theme.alpha(Theme.text, 0.04) : "transparent"
                        }
                        contentItem: Item {
                            Icon {
                                id: navIcon
                                x: sidebar.collapsed ? (parent.width - width) / 2 : 12
                                anchors.verticalCenter: parent.verticalCenter
                                name: modelData.icon || ""
                                size: 18
                                color: sidebar.current === modelData.key ? Theme.text : Theme.textMuted
                                scale: navButton.pressed ? 0.88 : 1
                                Behavior on x { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                                Behavior on scale { NumberAnimation { duration: Motion.fast } }
                            }
                            Text {
                                x: 44
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.label || ""
                                color: sidebar.current === modelData.key ? Theme.text : Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textMd
                                font.weight: sidebar.current === modelData.key ? Font.DemiBold : Font.Medium
                                opacity: sidebar.collapsed ? 0 : 1
                                visible: opacity > 0
                                Behavior on opacity { NumberAnimation { duration: Motion.fast } }
                            }
                            Rectangle {
                                readonly property int count: modelData.key === "library" ? Library.unreadUpdates
                                    : modelData.key === "downloads" ? Downloads.pending : 0
                                visible: count > 0
                                x: sidebar.collapsed ? navIcon.x + 12 : parent.width - width - 8
                                y: sidebar.collapsed ? 4 : (parent.height - height) / 2
                                width: sidebar.collapsed ? 8 : countText.implicitWidth + 12
                                height: sidebar.collapsed ? 8 : 20
                                radius: height / 2
                                color: Theme.accent
                                Text {
                                    id: countText
                                    anchors.centerIn: parent
                                    visible: !sidebar.collapsed
                                    text: parent.count > 99 ? "99+" : parent.count
                                    color: Theme.accentInk
                                    font.family: Theme.fontSans
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                }
                            }
                        }
                        ToolTip.visible: sidebar.collapsed && hovered
                        ToolTip.text: modelData.label || ""
                    }
                }
            }
        }
    }

    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        width: parent.width
        spacing: 2

        AbstractButton {
            id: settingsButton
            x: 12
            width: parent.width - 24
            height: 40
            hoverEnabled: true
            onClicked: sidebar.navigate("settings")
            background: Rectangle {
                radius: Theme.radiusMd
                color: sidebar.current === "settings" ? Theme.alpha(Theme.text, 0.08)
                     : settingsButton.hovered ? Theme.alpha(Theme.text, 0.04) : "transparent"
            }
            contentItem: Item {
                Icon {
                    x: sidebar.collapsed ? (parent.width - width) / 2 : 12
                    anchors.verticalCenter: parent.verticalCenter
                    name: "settings"
                    size: 18
                    color: sidebar.current === "settings" ? Theme.text : Theme.textMuted
                    rotation: settingsButton.hovered ? 45 : 0
                    Behavior on rotation { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                    Behavior on x { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                }
                Text {
                    x: 44
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Settings"
                    color: sidebar.current === "settings" ? Theme.text : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.Medium
                    opacity: sidebar.collapsed ? 0 : 1
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: Motion.fast } }
                }
            }
        }

        AbstractButton {
            id: collapseButton
            x: 12
            width: parent.width - 24
            height: 36
            hoverEnabled: true
            onClicked: App.setSetting("sidebarCollapsed", !sidebar.collapsed)
            background: Rectangle {
                radius: Theme.radiusMd
                color: collapseButton.hovered ? Theme.alpha(Theme.text, 0.04) : "transparent"
            }
            contentItem: Item {
                Icon {
                    x: sidebar.collapsed ? (parent.width - width) / 2 : 12
                    anchors.verticalCenter: parent.verticalCenter
                    name: "panel-left"
                    size: 17
                    color: Theme.textFaint
                    Behavior on x { NumberAnimation { duration: Motion.slow; easing.type: Easing.OutCubic } }
                }
                Text {
                    x: 44
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Collapse"
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    opacity: sidebar.collapsed ? 0 : 1
                    visible: opacity > 0
                }
            }
        }
    }
}
