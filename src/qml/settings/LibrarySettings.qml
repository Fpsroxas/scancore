import QtQuick
import ScanCore

SettingsPane {
    title: "Library"
    subtitle: "New chapters and notifications"
    iconName: "library"

    SettingsGroup {
        title: "Updates"
        SettingRow {
            iconName: "refresh-cw"
            title: "Check for new chapters"
            description: "Saved comics and novels are checked in the background while SCANCORE is open."
            Select {
                model: [{ value: 0, label: "Never" }, { value: 1, label: "Every hour" }, { value: 2, label: "Every 2 hours" },
                        { value: 6, label: "Every 6 hours" }, { value: 24, label: "Once a day" }]
                value: App.settings.libraryCheckHours
                onPicked: (v) => App.setSetting("libraryCheckHours", v)
            }
        }
        SettingRow {
            iconName: "bell"
            title: "Notify me about new chapters"
            Toggle { checked: App.settings.notifyUpdates !== false; onToggled: App.setSetting("notifyUpdates", checked) }
        }
    }

    SettingsGroup {
        visible: App.platform !== "android"
        title: "Background"
        SettingRow {
            iconName: "minimize"
            title: "Keep running in the tray when the window closes"
            description: "Lets update checks and downloads continue in the background."
            Toggle { checked: App.settings.closeToTray === true; onToggled: App.setSetting("closeToTray", checked) }
        }
    }
}
