import QtQuick
import ScanCore

SettingsPane {
    title: qsTr("Library")
    subtitle: qsTr("New chapters and notifications")
    iconName: "library"

    SettingsGroup {
        title: qsTr("Updates")
        SettingRow {
            iconName: "refresh-cw"
            title: qsTr("Check for new chapters")
            description: qsTr("Saved comics and novels are checked in the background while SCANCORE is open.")
            Select {
                model: [{ value: 0, label: qsTr("Never") }, { value: 1, label: qsTr("Every hour") }, { value: 2, label: qsTr("Every 2 hours") },
                        { value: 6, label: qsTr("Every 6 hours") }, { value: 24, label: qsTr("Once a day") }]
                value: App.settings.libraryCheckHours
                onPicked: (v) => App.setSetting("libraryCheckHours", v)
            }
        }
        SettingRow {
            iconName: "bell"
            title: qsTr("Notify me about new chapters")
            Toggle { checked: App.settings.notifyUpdates !== false; onToggled: App.setSetting("notifyUpdates", checked) }
        }
    }

    SettingsGroup {
        visible: App.platform !== "android"
        title: qsTr("Background")
        SettingRow {
            iconName: "minimize"
            title: qsTr("Keep running in the tray when the window closes")
            description: qsTr("Lets update checks and downloads continue in the background.")
            Toggle { checked: App.settings.closeToTray === true; onToggled: App.setSetting("closeToTray", checked) }
        }
    }
}
