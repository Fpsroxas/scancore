import QtQuick
import ScanCore

SettingsPane {
    id: pane
    title: "About"
    subtitle: "Version, updates and help"
    iconName: "info"

    readonly property string updateLine: Updater.managedByFlatpak ? "Updates come through Flatpak."
        : Updater.state === "checking" ? "Checking for updates…"
        : Updater.state === "current" ? "You have the latest version."
        : Updater.state === "available" ? "Version " + Updater.version + " is available."
        : Updater.state === "downloading" ? "Downloading " + Updater.version + " · " + Math.round(Updater.progress * 100) + "%"
        : Updater.state === "ready" ? "Version " + Updater.version + " is ready to install."
        : Updater.state === "error" ? Updater.error
        : "Free and open source, by Medium Dev."

    // A key drawn as a keycap.
    component Key: Rectangle {
        property alias text: label.text
        width: Math.max(26, label.implicitWidth + 14)
        height: 24
        radius: 6
        color: Theme.surface2
        border.width: 1
        border.color: Theme.borderStrong
        Text {
            id: label
            anchors.centerIn: parent
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs + 1
            font.weight: Font.DemiBold
        }
    }

    Rectangle {
        width: parent.width
        height: card.height + 40
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        Flow {
            id: card
            x: 20
            y: 20
            width: parent.width - 40
            spacing: 16
            Row {
                spacing: 16
                width: Theme.compact ? card.width : Math.max(0, card.width - updateButtons.width - 16)
                Rectangle {
                    width: 56
                    height: 56
                    radius: 16
                    color: Theme.dark ? "#000000" : "#141416"
                    border.width: 1
                    border.color: Theme.borderStrong
                    Icon { anchors.centerIn: parent; name: "scancore-mark"; size: 30; color: "#ffffff" }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 72
                    spacing: 4
                    Row {
                        spacing: 8
                        Text {
                            text: "SCANCORE"
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.textXl
                            font.weight: Font.Bold
                            font.letterSpacing: 1.5
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: version.implicitWidth + 14
                            height: 20
                            radius: 10
                            color: Theme.surface2
                            border.width: 1
                            border.color: Theme.border
                            Text {
                                id: version
                                anchors.centerIn: parent
                                text: App.version
                                color: Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textXs
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: pane.updateLine
                        color: Updater.state === "error" ? Theme.danger : Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        wrapMode: Text.Wrap
                    }
                }
            }
            Row {
                id: updateButtons
                visible: !Updater.managedByFlatpak
                spacing: 8
                ScButton {
                    visible: Updater.state === "available" || Updater.state === "ready"
                    variant: "primary"
                    text: Updater.state === "ready" ? "Restart to update" : Updater.canInstall ? "Update" : "Download"
                    onClicked: Updater.canInstall ? (Updater.state === "ready" ? Updater.install() : Updater.download()) : App.openUrl(Updater.pageUrl)
                }
                ScButton {
                    text: "Check for updates"
                    iconName: "refresh-cw"
                    loading: Updater.state === "checking"
                    enabled: Updater.state !== "downloading"
                    onClicked: Updater.check(false)
                }
            }
        }
    }

    SettingsGroup {
        visible: !Updater.managedByFlatpak
        title: "Updates"
        SettingRow {
            title: "Check for app updates"
            Toggle { checked: App.settings.checkAppUpdates !== false; onToggled: App.setSetting("checkAppUpdates", checked) }
        }
        SettingRow {
            title: "Install updates automatically"
            description: Updater.canInstall ? "New versions download in the background and install when you restart."
                : "This copy cannot update itself (for example a portable or source build); you will be told when a new version is out."
            Toggle { checked: App.settings.autoUpdate !== false; onToggled: App.setSetting("autoUpdate", checked) }
        }
    }

    SettingsGroup {
        title: "Help and community"
        SettingRow { iconName: "globe"; title: "scancore.onl"; description: "The SCANCORE website"; clickable: true; external: true; onClicked: App.openUrl("https://scancore.onl") }
        SettingRow { iconName: "message-circle"; title: "Discord"; description: "Questions, feedback and updates"; clickable: true; external: true; onClicked: App.openUrl("https://scancore.onl/discord") }
        SettingRow { iconName: "triangle-alert"; title: "Report a problem"; description: "Opens GitHub Issues"; clickable: true; external: true; onClicked: App.openUrl("https://github.com/medium-gh/scancore/issues/new/choose") }
        SettingRow { iconName: "sparkles"; title: "Welcome tour"; description: "See the introduction again"; clickable: true; onClicked: Router.shell.showWelcome() }
    }

    SettingsGroup {
        visible: !Theme.compact
        title: "Keyboard shortcuts"
        Grid {
            x: 16
            width: parent.width - 32
            topPadding: 14
            bottomPadding: 14
            columns: 2
            columnSpacing: 28
            rowSpacing: 10
            Repeater {
                model: [{ keys: ["Ctrl", "K"], label: "Search everything" }, { keys: ["Esc"], label: "Back, or close the reader" },
                        { keys: ["Ctrl", "1–5"], label: "Switch section" }, { keys: ["Alt", "←"], label: "Back" },
                        { keys: ["Ctrl", "B"], label: "Collapse the sidebar" }, { keys: ["F11"], label: "Full screen" },
                        { keys: ["N", "P"], label: "Next or previous chapter" }, { keys: ["M"], label: "Strip or pages (comics)" },
                        { keys: ["Space"], label: "Scroll a screen" }, { keys: ["+", "−"], label: "Width or text size" }]
                Row {
                    required property var modelData
                    width: (parent.width - 28) / 2
                    spacing: 10
                    Row {
                        width: 92
                        spacing: 4
                        Repeater {
                            model: modelData.keys
                            Key { required property string modelData; text: modelData }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 102
                        text: modelData.label
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Text {
        width: parent.width
        leftPadding: 4
        wrapMode: Text.Wrap
        text: "MIT licence · github.com/medium-gh/scancore. Fonts: DM Sans, Space Grotesk, Literata (OFL). Icons: Lucide (ISC). HTML parsing: lexbor (Apache 2.0)."
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.textXs + 1
    }
}
