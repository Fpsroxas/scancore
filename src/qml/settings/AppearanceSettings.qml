import QtQuick
import QtQuick.Controls.Basic
import ScanCore

SettingsPane {
    id: pane
    title: "Appearance"
    subtitle: "Theme, accent colour and motion"
    iconName: "palette"

    // Four across when there is room, otherwise two.
    readonly property int tileWidth: Theme.compact || width < 560 ? Math.floor((width - 36) / 2)
                                                                 : Math.min(168, Math.floor((width - 60) / 4))

    // A tiny window drawn in a theme's own colours.
    component ThemePreview: Item {
        property var colors: Theme.palettes.dark
        anchors.fill: parent
        Rectangle {
            anchors.fill: parent
            color: colors.bg
        }
        Rectangle {
            width: parent.width * 0.28
            height: parent.height
            color: colors.sidebar
            Column {
                x: 8
                y: 10
                spacing: 6
                Repeater {
                    model: 4
                    Rectangle { width: 22 - index * 3; height: 4; radius: 2; color: index === 0 ? colors.text : colors.textFaint }
                }
            }
        }
        Column {
            x: parent.width * 0.28 + 10
            y: 10
            spacing: 6
            Rectangle { width: 54; height: 6; radius: 3; color: colors.text }
            Row {
                spacing: 5
                Repeater {
                    model: 3
                    Rectangle {
                        width: 24
                        height: 34
                        radius: 4
                        color: colors.surface3
                        border.width: 1
                        border.color: colors.border
                    }
                }
            }
            Rectangle { width: 40; height: 4; radius: 2; color: colors.textFaint }
        }
    }

    SettingsGroup {
        title: "Theme"
        Flow {
            x: 12
            width: parent.width - 24
            topPadding: 12
            bottomPadding: 12
            spacing: 12
            Repeater {
                model: [{ value: "system", label: "System", detail: "Follows your device" },
                        { value: "light", label: "Light" }, { value: "dark", label: "Dark" },
                        { value: "black", label: "Black", detail: "For OLED screens" }]
                ChoiceTile {
                    required property var modelData
                    width: pane.tileWidth
                    label: modelData.label
                    detail: modelData.detail || ""
                    selected: App.settings.theme === modelData.value
                    onClicked: App.setSetting("theme", modelData.value)
                    ThemePreview {
                        visible: modelData.value !== "system"
                        colors: Theme.palettes[modelData.value] || Theme.palettes.dark
                    }
                    // System: light and dark, split down the middle.
                    Item {
                        anchors.fill: parent
                        visible: modelData.value === "system"
                        ThemePreview { colors: Theme.palettes.light }
                        Item {
                            x: parent.width / 2
                            width: parent.width / 2
                            height: parent.height
                            clip: true
                            ThemePreview {
                                anchors.fill: undefined
                                x: -parent.x
                                width: parent.parent.width
                                height: parent.height
                                colors: Theme.palettes.dark
                            }
                        }
                    }
                }
            }
        }
    }

    SettingsGroup {
        title: "Accent"
        note: "Mono matches the scancore.onl website."
        Flow {
            x: 16
            width: parent.width - 32
            topPadding: 16
            bottomPadding: 14
            spacing: Theme.compact ? 14 : 22
            Repeater {
                model: [{ value: "mono", label: "Mono" }, { value: "violet", label: "Violet" }, { value: "ember", label: "Ember" },
                        { value: "sky", label: "Sky" }, { value: "rose", label: "Rose" }]
                AbstractButton {
                    id: swatch
                    required property var modelData
                    readonly property bool selected: (App.settings.accent || "mono") === modelData.value
                    width: 52
                    height: 62
                    hoverEnabled: true
                    onClicked: App.setSetting("accent", modelData.value)
                    contentItem: Column {
                        spacing: 7
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 36
                            height: 36
                            radius: 18
                            color: "transparent"
                            border.width: 2
                            border.color: swatch.selected ? Theme.text : swatch.hovered ? Theme.borderStrong : "transparent"
                            Rectangle {
                                anchors.centerIn: parent
                                width: 28
                                height: 28
                                radius: 14
                                color: Theme.dark ? Theme.accents[swatch.modelData.value].dark : Theme.accents[swatch.modelData.value].light
                                Icon {
                                    anchors.centerIn: parent
                                    visible: swatch.selected
                                    name: "check"
                                    size: 14
                                    color: Theme.dark ? Theme.accents[swatch.modelData.value].on.dark : Theme.accents[swatch.modelData.value].on.light
                                }
                            }
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: swatch.modelData.label
                            color: swatch.selected ? Theme.text : Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textXs + 1
                            font.weight: swatch.selected ? Font.DemiBold : Font.Normal
                        }
                    }
                    background: null
                }
            }
        }
    }

    SettingsGroup {
        title: "Motion"
        SettingRow {
            iconName: "sparkles"
            title: "Reduce motion"
            description: "Turns off page transitions, hover lifts and other animations."
            Toggle { checked: App.settings.reducedMotion === true; onToggled: App.setSetting("reducedMotion", checked) }
        }
    }
}
