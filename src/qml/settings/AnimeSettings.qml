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
            iconName: "clock"
            title: "Countdown before the next episode"
            enabled: App.settings.autoNext !== false
            opacity: enabled ? 1 : 0.45
            Segmented {
                options: [{ value: 3, label: "3 s" }, { value: 8, label: "8 s" }, { value: 15, label: "15 s" }]
                value: Number(App.settings.nextCountdown) || 8
                onPicked: (v) => App.setSetting("nextCountdown", v)
            }
        }
        SettingRow {
            iconName: "rotate-ccw"
            title: "Skip intros and recaps automatically"
            description: "Otherwise a Skip button appears while they play."
            Toggle { checked: App.settings.autoSkip === true; onToggled: App.setSetting("autoSkip", checked) }
        }
        SettingRow {
            iconName: "skip-forward"
            title: "Skip endings automatically"
            description: "Goes straight to the next episode's countdown when the credits start."
            Toggle { checked: App.settings.autoSkipEndings === true; onToggled: App.setSetting("autoSkipEndings", checked) }
        }
    }
}
