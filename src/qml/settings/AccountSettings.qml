import QtQuick
import QtQuick.Controls.Basic
import ScanCore

SettingsPane {
    id: pane
    title: "Account"
    subtitle: "Optional sync with scancore.onl"
    iconName: "user-round"

    readonly property string name: Account.user.username || Account.user.email || ""
    readonly property string avatarUrl: {
        const a = Account.user.avatar || ""
        return a.startsWith("/") ? Account.site + a : a
    }
    readonly property string syncLine: Account.syncing ? "Syncing…"
        : Account.lastSync ? "Synced " + App.relativeTime(Number(Account.lastSync))
        : "Not synced yet"

    function report(task) {
        task.finished.connect((r) => Router.toast("Synced · " + r.libraryImported + " saved, " + r.historyImported + " progress updates in, "
                                                  + r.pushed + " sent", "success"))
        task.failed.connect((error) => Router.toast(error, "error"))
    }

    // Signed in: who, when it last synced, and the actions.
    Rectangle {
        visible: Account.signedIn
        width: parent.width
        height: profile.height + 36
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        Flow {
            id: profile
            x: 18
            y: 18
            width: parent.width - 36
            spacing: 16

            Row {
                spacing: 14
                width: Theme.compact ? profile.width : Math.max(0, profile.width - actions.width - 16)
                Rectangle {
                    width: 52
                    height: 52
                    radius: 26
                    color: Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: pane.name.slice(0, 1).toUpperCase()
                        color: Theme.accentInk
                        font.family: Theme.fontDisplay
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                    Cover {
                        id: avatar
                        anchors.fill: parent
                        visible: pane.avatarUrl.length > 0 && ready
                        source: pane.avatarUrl
                        radius: 26
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    width: parent.width - 66
                    Text {
                        width: parent.width
                        text: pane.name
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textLg
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Row {
                        spacing: 6
                        Spinner { visible: Account.syncing; size: 13; anchors.verticalCenter: parent.verticalCenter }
                        Icon { visible: !Account.syncing; name: "cloud-check"; size: 14; color: Theme.success; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            text: pane.syncLine + " · scancore.onl"
                            color: Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textSm
                        }
                    }
                }
            }
            Row {
                id: actions
                spacing: 8
                ScButton {
                    variant: "primary"
                    text: "Sync now"
                    iconName: "refresh-cw"
                    loading: Account.syncing
                    enabled: !Account.syncing
                    onClicked: pane.report(Account.sync())
                }
                ScButton { text: "Sign out"; iconName: "log-out"; variant: "ghost"; onClicked: Account.signOut() }
            }
        }
    }

    // Signed out: what an account is for, then the form.
    Rectangle {
        visible: !Account.signedIn
        width: parent.width
        height: signIn.height + 40
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        Column {
            id: signIn
            x: 20
            y: 20
            width: parent.width - 40
            spacing: 18
            Row {
                spacing: 14
                width: parent.width
                Rectangle {
                    width: 44
                    height: 44
                    radius: 13
                    color: Theme.alpha(Theme.accent, 0.12)
                    Icon { anchors.centerIn: parent; name: "cloud"; size: 20; color: Theme.accent }
                }
                Column {
                    width: parent.width - 58
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    Text {
                        text: "Sync with scancore.onl"
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textLg
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Keep your library and anime progress the same on every device and on the website. Everything works without an account."
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.textSm
                    }
                }
            }
            AuthForm {
                width: Math.min(parent.width, 420)
                onDone: Router.toast("Signed in", "success")
            }
        }
    }
}
