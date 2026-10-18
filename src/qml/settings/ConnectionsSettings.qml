import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import ScanCore

// Other services: lists on AniList and MyAnimeList that follow what you
// finish, Discord showing what you read, and libraries brought in from
// Mihon, AniList or MyAnimeList.
SettingsPane {
    id: pane
    title: qsTr("Connections")
    subtitle: qsTr("Tracking and imports")
    iconName: "link"

    property bool pastingToken: false
    property var malTask: null
    property var comickTask: null
    property var importTask: null
    property string importing: ""
    property var matchTask: null
    property bool showAllWaiting: false
    readonly property var waiting: Importer.unmatched

    function imported(task, what) {
        pane.importTask = task
        pane.importing = what
        task.finished.connect((r) => {
            const parts = [r.imported === 1 ? qsTr("1 series added") : r.imported + qsTr(" series added")]
            if (r.progress > 0) parts.push(r.progress + (r.progress === 1 ? qsTr(" chapter or episode") : qsTr(" chapters and episodes")) + qsTr(" marked"))
            if (r.unmatched > 0) parts.push(r.unmatched + qsTr(" waiting to be found"))
            Router.toast(parts.join(" · "), "success")
        })
        task.failed.connect((error) => Router.toast(error, "error"))
    }

    // ------------------------------------------------------------ tracking

    SettingsGroup {
        title: qsTr("Tracking")
        note: Trackers.anilistAvailable || Trackers.malAvailable || Trackers.comickAvailable
            ? qsTr("Finishing a chapter or episode moves the series forward on your list. Anime are matched by their AniList entry and ")
              + "comics and novels by title; link one by hand from its page."
            : qsTr("This copy of SCANCORE was built without AniList, MyAnimeList and Comick ids, so tracking is off. Official downloads have them.")

        SettingRow {
            visible: Trackers.anilistAvailable
            iconName: "list"
            iconTint: Theme.dark ? "#3db4f2" : "#0b7fc0"
            title: "AniList"
            description: Trackers.anilist.name ? qsTr("Connected as ") + Trackers.anilist.name
                : pane.pastingToken ? qsTr("Sign in on the page that opened, then paste the token it shows.")
                : qsTr("Keep your AniList lists up to date.")
            Row {
                spacing: 8
                ScButton {
                    visible: !Trackers.anilist.name
                    variant: pane.pastingToken ? "ghost" : "primary"
                    text: pane.pastingToken ? qsTr("Open again") : qsTr("Connect")
                    onClicked: {
                        Trackers.anilistSignIn()
                        pane.pastingToken = true
                    }
                }
                ScButton {
                    visible: !!Trackers.anilist.name
                    text: qsTr("Disconnect")
                    variant: "ghost"
                    onClicked: Trackers.signOut("anilist")
                }
            }
        }
        // The token AniList shows after signing in.
        Item {
            visible: Trackers.anilistAvailable && pane.pastingToken && !Trackers.anilist.name
            width: parent.width
            height: tokenRow.height + 20
            Row {
                id: tokenRow
                x: 62
                width: parent.width - x - 16
                spacing: 8
                property var task: null
                InlineField {
                    id: tokenField
                    width: parent.width - useToken.width - 8
                    placeholderText: qsTr("Paste the token")
                    echoMode: TextInput.Password
                    onAccepted: useToken.clicked()
                }
                ScButton {
                    id: useToken
                    variant: "primary"
                    text: qsTr("Use token")
                    loading: tokenRow.task !== null && tokenRow.task.loading
                    enabled: tokenField.text.trim().length > 0
                    onClicked: {
                        tokenRow.task = Trackers.anilistUseToken(tokenField.text)
                        tokenRow.task.finished.connect((r) => {
                            tokenField.text = ""
                            pane.pastingToken = false
                            Router.toast(qsTr("Connected to AniList as ") + r.name, "success")
                        })
                        tokenRow.task.failed.connect((error) => Router.toast(error, "error"))
                    }
                }
            }
        }
        SettingRow {
            visible: Trackers.malAvailable
            iconName: "bookmark"
            iconTint: Theme.dark ? "#6d8fe0" : "#2e51a2"
            title: "MyAnimeList"
            description: Trackers.mal.name ? qsTr("Connected as ") + Trackers.mal.name
                : pane.malTask && pane.malTask.loading ? qsTr("Waiting for you to sign in on the page that opened…")
                : qsTr("Keep your MyAnimeList lists up to date.")
            Row {
                spacing: 8
                ScButton {
                    visible: !Trackers.mal.name
                    variant: "primary"
                    text: pane.malTask && pane.malTask.loading ? qsTr("Cancel") : qsTr("Connect")
                    onClicked: {
                        if (pane.malTask && pane.malTask.loading) {
                            pane.malTask.cancel()
                            return
                        }
                        pane.malTask = Trackers.malSignIn()
                        pane.malTask.finished.connect((r) => Router.toast(qsTr("Connected to MyAnimeList as ") + r.name, "success"))
                        pane.malTask.failed.connect((error) => { if (error !== "Cancelled") Router.toast(error, "error") })
                    }
                }
                ScButton {
                    visible: !!Trackers.mal.name
                    text: qsTr("Disconnect")
                    variant: "ghost"
                    onClicked: Trackers.signOut("mal")
                }
            }
        }
        SettingRow {
            visible: Trackers.comickAvailable
            iconName: "book-open"
            iconTint: Theme.dark ? "#a78bfa" : "#7c3aed"
            title: qsTr("Comick")
            description: Trackers.comick.name ? qsTr("Connected as ") + Trackers.comick.name
                : pane.comickTask && pane.comickTask.loading ? qsTr("Waiting for you to allow SCANCORE on the page that opened…")
                : qsTr("Keep your Comick library up to date, and bring it into SCANCORE.")
            Row {
                spacing: 8
                ScButton {
                    visible: !Trackers.comick.name
                    variant: "primary"
                    text: pane.comickTask && pane.comickTask.loading ? qsTr("Cancel") : qsTr("Connect")
                    onClicked: {
                        if (pane.comickTask && pane.comickTask.loading) {
                            pane.comickTask.cancel()
                            return
                        }
                        pane.comickTask = Trackers.comickSignIn()
                        pane.comickTask.finished.connect((r) => Router.toast(qsTr("Connected to Comick as ") + r.name, "success"))
                        pane.comickTask.failed.connect((error) => { if (error !== "Cancelled") Router.toast(error, "error") })
                    }
                }
                ScButton {
                    visible: !!Trackers.comick.name
                    text: qsTr("Disconnect")
                    variant: "ghost"
                    onClicked: Trackers.signOut("comick")
                }
            }
        }
    }

    // ------------------------------------------------------------ Discord

    SettingsGroup {
        visible: Presence.available
        title: qsTr("Discord")
        note: qsTr("Shown while a reader or the player is open, through the Discord app on this computer.")
        SettingRow {
            iconName: "message-circle"
            iconTint: Theme.dark ? "#8c9eff" : "#5865f2"
            title: qsTr("Show what you read and watch on Discord")
            description: App.settings.discordPresence !== true ? qsTr("Off") : Presence.connected ? qsTr("Connected to Discord") : qsTr("Discord is not running")
            Toggle { checked: App.settings.discordPresence === true; onToggled: App.setSetting("discordPresence", checked) }
        }
        SettingRow {
            visible: App.settings.discordPresence === true
            iconName: "eye-off"
            title: qsTr("Keep titles private")
            description: qsTr("Shows “Reading a comic” without saying which.")
            Toggle { checked: App.settings.discordHideTitles === true; onToggled: App.setSetting("discordHideTitles", checked) }
        }
    }

    // ------------------------------------------------------------ imports

    SettingsGroup {
        title: qsTr("Bring your library")
        note: qsTr("Anime come in with the episodes you watched. Comics and novels come in when one of your sources has them, ")
            + "with the chapters you read; the others wait below until they are found."
        SettingRow {
            iconName: "book-open"
            title: qsTr("Mihon or Tachiyomi backup")
            description: qsTr("A .tachibk file from More → Backup and restore.")
            ScButton {
                text: qsTr("Choose file")
                iconName: "folder-open"
                loading: pane.importing === "mihon" && pane.importTask !== null && pane.importTask.loading
                enabled: !(pane.importTask && pane.importTask.loading)
                onClicked: mihonDialog.open()
            }
        }
        SettingRow {
            iconName: "list"
            title: "AniList"
            description: qsTr("Anime, comics and novels from a public AniList list.")
            Row {
                spacing: 8
                InlineField {
                    id: anilistUser
                    width: Theme.compact ? 160 : 190
                    placeholderText: qsTr("User name")
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    onAccepted: anilistImport.clicked()
                }
                ScButton {
                    id: anilistImport
                    text: qsTr("Import")
                    loading: pane.importing === "anilist" && pane.importTask !== null && pane.importTask.loading
                    enabled: anilistUser.text.trim().length > 0 && !(pane.importTask && pane.importTask.loading)
                    onClicked: pane.imported(Importer.importAniList(anilistUser.text), "anilist")
                }
            }
        }
        SettingRow {
            visible: Trackers.comickAvailable
            iconName: "book-open"
            title: qsTr("Comick")
            description: Trackers.comick.name ? qsTr("Everything ") + Trackers.comick.name + qsTr(" follows on Comick.")
                : qsTr("Connect Comick above to bring in what you follow there.")
            ScButton {
                text: qsTr("Import")
                enabled: !!Trackers.comick.name && !(pane.importTask && pane.importTask.loading)
                loading: pane.importing === "comick" && pane.importTask !== null && pane.importTask.loading
                onClicked: pane.imported(Importer.importComick(), "comick")
            }
        }
        SettingRow {
            iconName: "bookmark"
            title: "MyAnimeList"
            description: qsTr("The anime or manga list file from MyAnimeList's export page.")
            Row {
                spacing: 8
                ScButton {
                    text: qsTr("Export page")
                    iconName: "external-link"
                    variant: "ghost"
                    onClicked: App.openUrl("https://myanimelist.net/panel.php?go=export")
                }
                ScButton {
                    text: qsTr("Choose file")
                    iconName: "folder-open"
                    loading: pane.importing === "mal" && pane.importTask !== null && pane.importTask.loading
                    enabled: !(pane.importTask && pane.importTask.loading)
                    onClicked: malDialog.open()
                }
            }
        }
    }

    FileDialog {
        id: mihonDialog
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Mihon or Tachiyomi backup (*.tachibk *.proto.gz)"), qsTr("All files (*)")]
        onAccepted: pane.imported(Importer.importMihon(selectedFile), "mihon")
    }
    FileDialog {
        id: malDialog
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("MyAnimeList export (*.xml *.xml.gz *.gz)"), qsTr("All files (*)")]
        onAccepted: pane.imported(Importer.importMal(selectedFile), "mal")
    }

    // Titles from an import that no source had yet.
    SettingsGroup {
        visible: pane.waiting.length > 0
        title: qsTr("Waiting to be found · ") + pane.waiting.length
        note: qsTr("Adding a source in Settings → Sources, or a site coming back, can find more of them later.")
        SettingRow {
            iconName: "search"
            title: qsTr("Look for them on every source")
            description: pane.matchTask && pane.matchTask.loading ? qsTr("Searching… this can take a few minutes.")
                : qsTr("Adds the ones a source has under exactly the same title.")
            ScButton {
                variant: "primary"
                text: pane.matchTask && pane.matchTask.loading ? qsTr("Stop") : qsTr("Find them")
                onClicked: {
                    if (pane.matchTask && pane.matchTask.loading) {
                        pane.matchTask.cancel()
                        return
                    }
                    pane.matchTask = Importer.matchAll()
                    pane.matchTask.finished.connect((r) => Router.toast(r.matched > 0
                        ? qsTr("Found ") + r.matched + (r.left > 0 ? " · " + r.left + qsTr(" still waiting") : "") : qsTr("None found yet"), r.matched > 0 ? "success" : "info"))
                    pane.matchTask.failed.connect((error) => { if (error !== "Cancelled") Router.toast(error, "error") })
                }
            }
        }
        Repeater {
            model: pane.showAllWaiting ? pane.waiting : pane.waiting.slice(0, 6)
            SettingRow {
                required property var modelData
                iconName: modelData.media === "novel" ? "book-text" : "book-open"
                title: modelData.title
                description: (modelData.media === "novel" ? qsTr("Novel") : qsTr("Comic"))
                    + (modelData.read > 0 ? " · " + modelData.read + qsTr(" read") : "")
                    + (modelData.source ? qsTr(" · from ") + modelData.source : "")
                IconButton {
                    iconName: "x"
                    tip: qsTr("Forget")
                    onClicked: Importer.forget(modelData.title)
                }
            }
        }
        Item {
            width: parent.width
            height: 56
            Rectangle { x: 16; width: parent.width - 16; height: 1; color: Theme.border }
            ScButton {
                visible: pane.waiting.length > 6 && !pane.showAllWaiting
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Show all ") + pane.waiting.length
                variant: "ghost"
                compact: true
                onClicked: pane.showAllWaiting = true
            }
            ScButton {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Forget all")
                variant: "ghost"
                compact: true
                onClicked: forgetConfirm.open()
            }
        }
    }

    ConfirmDialog {
        id: forgetConfirm
        heading: qsTr("Forget these titles?")
        message: "They stay wherever you imported them from; SCANCORE stops looking for them."
        confirmText: qsTr("Forget all")
        destructive: true
        onConfirmed: Importer.forgetAll()
    }

    // A small single-line input for a row.
    component InlineField: TextField {
        implicitHeight: 38
        leftPadding: 14
        rightPadding: 14
        color: Theme.text
        placeholderTextColor: Theme.textFaint
        selectionColor: Theme.alpha(Theme.accent, 0.35)
        font.family: Theme.fontSans
        font.pixelSize: Theme.textMd
        selectByMouse: true
        verticalAlignment: TextInput.AlignVCenter
        background: Rectangle {
            radius: height / 2
            color: parent.activeFocus ? Theme.surface : Theme.surface2
            border.width: 1
            border.color: parent.activeFocus ? Theme.alpha(Theme.accent, 0.55) : Theme.border
        }
    }
}
