import QtQuick
import QtQuick.Controls.Basic
import ScanCore

SettingsPane {
    id: pane
    title: "Interface"
    subtitle: "Navigation, size and motion"
    iconName: "layout-grid"

    readonly property string navStyle: ["sidebar", "floating", "rail", "dock"].indexOf(App.settings.navStyle) >= 0
        ? App.settings.navStyle : "sidebar"
    readonly property bool rightSide: App.settings.navSide === "right"
    readonly property bool topSide: App.settings.dockSide === "top"
    readonly property int tileWidth: Theme.compact || width < 600 ? Math.floor((width - 36) / 2)
                                                                 : Math.min(168, Math.floor((width - 60) / 4))

    // A small window showing where navigation sits.
    component LayoutPreview: Item {
        id: preview
        property string kind: "sidebar"
        anchors.fill: parent
        readonly property color panel: Theme.surface3
        readonly property color line: Theme.borderStrong
        readonly property bool mirrored: pane.rightSide
        Rectangle { anchors.fill: parent; color: Theme.surface2 }
        // Content lines.
        Column {
            x: preview.kind === "dock" ? 14 : preview.mirrored ? 10 : (preview.kind === "rail" ? 30 : 52)
            y: preview.kind === "dock" && pane.topSide ? 30 : 12
            spacing: 6
            Rectangle { width: 46; height: 6; radius: 3; color: preview.line }
            Row {
                spacing: 5
                Repeater {
                    model: preview.kind === "dock" ? 5 : 3
                    Rectangle { width: 18; height: 26; radius: 3; color: preview.panel }
                }
            }
        }
        // Sidebars.
        Rectangle {
            visible: preview.kind !== "dock"
            readonly property int inset: preview.kind === "floating" ? 5 : 0
            x: preview.mirrored ? parent.width - width - inset : inset
            y: inset
            width: preview.kind === "rail" ? 18 : 40
            height: parent.height - 2 * inset
            radius: preview.kind === "floating" ? 6 : 0
            color: preview.panel
            border.width: preview.kind === "floating" ? 1 : 0
            border.color: preview.line
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                spacing: 5
                Repeater {
                    model: 4
                    Rectangle {
                        width: preview.kind === "rail" ? 8 : 26 - index * 3
                        height: preview.kind === "rail" ? 8 : 4
                        radius: preview.kind === "rail" ? 4 : 2
                        color: index === 0 ? Theme.text : preview.line
                    }
                }
            }
        }
        // Dock.
        Rectangle {
            visible: preview.kind === "dock"
            anchors.horizontalCenter: parent.horizontalCenter
            y: pane.topSide ? 5 : parent.height - height - 5
            width: parent.width * 0.62
            height: 14
            radius: 7
            color: preview.panel
            border.width: 1
            border.color: preview.line
            Row {
                anchors.centerIn: parent
                spacing: 5
                Repeater {
                    model: 6
                    Rectangle { width: 5; height: 5; radius: 2.5; color: index === 1 ? Theme.text : preview.line }
                }
            }
        }
    }

    SettingsGroup {
        visible: App.platform !== "android"
        title: "Navigation"
        note: "Narrow windows and phones always use tabs along the bottom."
        Flow {
            x: 12
            width: parent.width - 24
            topPadding: 12
            bottomPadding: 12
            spacing: 12
            Repeater {
                model: [{ value: "sidebar", label: "Sidebar", detail: "Full height" },
                        { value: "floating", label: "Floating", detail: "Inset panel" },
                        { value: "rail", label: "Rail", detail: "Icons only" },
                        { value: "dock", label: "Dock", detail: "A bar of icons" }]
                ChoiceTile {
                    required property var modelData
                    width: pane.tileWidth
                    label: modelData.label
                    detail: modelData.detail
                    selected: pane.navStyle === modelData.value
                    onClicked: App.setSetting("navStyle", modelData.value)
                    LayoutPreview { kind: modelData.value }
                }
            }
        }
        SettingRow {
            iconName: "panel-left"
            title: pane.navStyle === "dock" ? "Dock position" : "Sidebar position"
            Segmented {
                visible: pane.navStyle !== "dock"
                options: [{ value: "left", label: "Left" }, { value: "right", label: "Right" }]
                value: pane.rightSide ? "right" : "left"
                onPicked: (v) => App.setSetting("navSide", v)
            }
            Segmented {
                visible: pane.navStyle === "dock"
                options: [{ value: "bottom", label: "Bottom" }, { value: "top", label: "Top" }]
                value: pane.topSide ? "top" : "bottom"
                onPicked: (v) => App.setSetting("dockSide", v)
            }
        }
    }

    SettingsGroup {
        title: "Size"
        SettingRow {
            iconName: "type"
            title: "Text size"
            description: "Titles, labels and descriptions throughout the app."
            Segmented {
                options: [{ value: 0.92, label: "Small" }, { value: 1, label: "Default" },
                          { value: 1.1, label: "Large" }, { value: 1.2, label: "Larger" }]
                value: Theme.textScale
                onPicked: (v) => App.setSetting("textScale", v)
            }
        }
        SettingRow {
            iconName: "layout-grid"
            title: "Cover size"
            description: "How big covers are in grids and rows. Smaller fits more on screen."
            Segmented {
                options: [{ value: "small", label: "Small" }, { value: "default", label: "Default" }, { value: "large", label: "Large" }]
                value: App.settings.coverSize || "default"
                onPicked: (v) => App.setSetting("coverSize", v)
            }
        }
    }

    SettingsGroup {
        title: "Motion"
        SettingRow {
            iconName: "gauge"
            title: "Animation speed"
            description: "How quickly pages, menus and covers move."
            enabled: !Motion.reduced
            opacity: enabled ? 1 : 0.45
            Segmented {
                options: [{ value: "relaxed", label: "Relaxed" }, { value: "default", label: "Default" }, { value: "snappy", label: "Snappy" }]
                value: App.settings.motionSpeed || "default"
                onPicked: (v) => App.setSetting("motionSpeed", v)
            }
        }
        SettingRow {
            iconName: "sparkles"
            title: "Reduce motion"
            description: "Turns off page transitions, hover lifts, animated backgrounds and other animations."
            Toggle { checked: App.settings.reducedMotion === true; onToggled: App.setSetting("reducedMotion", checked) }
        }
    }

    SettingsGroup {
        title: "Start"
        SettingRow {
            iconName: "house"
            title: "Open SCANCORE on"
            description: "The page you see when the app starts."
            Select {
                model: [{ value: "home", label: "Home" }, { value: "comics", label: "Comics" }, { value: "novels", label: "Novels" },
                        { value: "anime", label: "Anime" }, { value: "library", label: "Library" }, { value: "history", label: "History" }]
                value: App.settings.startSection || "home"
                onPicked: (v) => App.setSetting("startSection", v)
            }
        }
    }
}
