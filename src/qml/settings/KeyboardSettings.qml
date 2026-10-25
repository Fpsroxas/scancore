import QtQuick
import QtQuick.Controls.Basic
import Scancore

SettingsPane {
    id: pane
    title: qsTr("Keyboard")
    subtitle: qsTr("Shortcuts for the app, readers and player")
    iconName: "keyboard"

    // Gives `id` the keys `sequence`, taking them from an action that
    // already answers to them.
    function assign(id, sequence) {
        const other = Keymap.conflict(id, sequence)
        if (other) {
            Keymap.set(other.id, Keymap.sequences(other.id).filter(s => !Customize.sameKeys(s, sequence)))
            Router.toast(Customize.keyText(sequence) + qsTr(" moved from “") + other.label + "”")
        }
        Keymap.set(id, [sequence])
    }

    SettingsGroup {
        title: qsTr("Shortcuts")
        note: "Click a shortcut, then press the new keys. Backspace leaves an action without one; Esc keeps what it had. "
            + qsTr("Escape, the arrow keys and scrolling keys always work as they do.")
        SettingRow {
            iconName: "keyboard"
            title: qsTr("Every shortcut on one sheet")
            description: qsTr("Press ") + (Keymap.labels("shortcuts")[0] || "?") + qsTr(" anywhere to see them.")
            Row {
                spacing: 8
                ScButton { text: qsTr("Show"); onClicked: Router.shell.openShortcuts() }
                ScButton {
                    text: qsTr("Reset all")
                    variant: "ghost"
                    enabled: Object.keys(Keymap.changed).length > 0
                    onClicked: Keymap.resetAll()
                }
            }
        }
    }

    Repeater {
        model: Keymap.scopes
        SettingsGroup {
            required property var modelData
            title: modelData.title
            Repeater {
                model: Keymap.actions.filter(a => a.scope === modelData.scope)
                SettingRow {
                    id: row
                    required property var modelData
                    title: modelData.label
                    description: Keymap.isDefault(modelData.id) ? "" : qsTr("Default: ")
                        + (modelData.keys.length ? modelData.keys.map(k => Customize.keyText(k)).join(", ") : "none")
                    Row {
                        spacing: 4
                        KeyRecorder {
                            labels: Keymap.labels(row.modelData.id)
                            onCaptured: (sequence) => pane.assign(row.modelData.id, sequence)
                            onCleared: Keymap.set(row.modelData.id, [])
                        }
                        IconButton {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !Keymap.isDefault(row.modelData.id)
                            iconName: "undo-2"
                            iconSize: 15
                            diameter: 32
                            tip: qsTr("Back to the default")
                            onClicked: Keymap.reset(row.modelData.id)
                        }
                    }
                }
            }
        }
    }
}
