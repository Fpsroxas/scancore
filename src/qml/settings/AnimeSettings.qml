import QtQuick
import ScanCore

SettingsPane {
    title: "Anime"
    subtitle: "Playback and skipping"
    iconName: "tv"

    SettingsGroup {
        title: "Player"
        note: "Episodes resume where you stopped. Skip times come from AniSkip when someone has submitted them."
        SettingRow {
            iconName: "skip-forward"
            title: "Play the next episode automatically"
            description: "Starts after a short countdown you can cancel."
            Toggle { checked: App.settings.autoNext !== false; onToggled: App.setSetting("autoNext", checked) }
        }
        SettingRow {
            iconName: "rotate-ccw"
            title: "Skip intros and recaps automatically"
            description: "Otherwise a Skip button appears while they play."
            Toggle { checked: App.settings.autoSkip === true; onToggled: App.setSetting("autoSkip", checked) }
        }
    }
}
