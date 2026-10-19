import QtQuick
import ScanCore

SettingsPane {
    title: qsTr("Library")
    subtitle: qsTr("New chapters, notifications and background work")
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
            description: qsTr("They are always listed under Notifications; this also shows them in the system's notifications.")
            Toggle { checked: App.settings.notifyUpdates !== false; onToggled: App.setSetting("notifyUpdates", checked) }
        }
        SettingRow {
            iconName: "megaphone"
            title: qsTr("Announcements from SCANCORE")
            description: qsTr("News about SCANCORE from scancore.onl, in Notifications as soon as they are posted.")
            Toggle { checked: App.settings.announcements !== false; onToggled: App.setSetting("announcements", checked) }
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

    SettingsGroup {
        visible: App.platform === "android"
        title: qsTr("Background")
        SettingRow {
            iconName: App.batteryRestricted ? "battery-warning" : "check"
            title: App.batteryRestricted ? qsTr("Battery optimisation stops SCANCORE in the background")
                                         : qsTr("SCANCORE can run in the background")
            description: App.batteryRestricted
                ? qsTr("Android closes it soon after you leave it, so new chapters are not checked, downloads pause and reading aloud stops.")
                : qsTr("Update checks, downloads and reading aloud carry on when you leave the app.")
            ScButton {
                visible: App.batteryRestricted
                text: qsTr("Allow")
                onClicked: App.askBatteryUnrestricted()
            }
        }
    }
}
