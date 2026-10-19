import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Connect and Disconnect for one tracker (anilist, mal or comick), with
// AniList's step of pasting the token its sign-in page shows. Used in
// Settings → Connections and on the first-run tracker step.
Column {
    id: root
    property string service: "anilist"
    readonly property var account: service === "anilist" ? Trackers.anilist : service === "mal" ? Trackers.mal : Trackers.comick
    readonly property bool connected: !!account.name
    readonly property string label: service === "anilist" ? "AniList" : service === "mal" ? "MyAnimeList" : qsTr("Comick")
    property bool pastingToken: false
    property var task: null
    readonly property bool waiting: task !== null && task.loading
    // What is happening, for the row's description; empty when idle.
    readonly property string status: connected ? qsTr("Connected as ") + account.name
        : pastingToken ? qsTr("Sign in on the page that opened, then paste the token it shows.")
        : waiting ? (service === "comick" ? qsTr("Waiting for you to allow SCANCORE on the page that opened…")
                                          : qsTr("Waiting for you to sign in on the page that opened…"))
        : ""

    spacing: 10

    function connect() {
        if (service === "anilist") {
            Trackers.anilistSignIn()
            pastingToken = true
            return
        }
        if (waiting) {
            task.cancel()
            return
        }
        task = service === "mal" ? Trackers.malSignIn() : Trackers.comickSignIn()
        const name = label
        task.finished.connect((r) => Router.toast(qsTr("Connected to %1 as %2").arg(name).arg(r.name), "success"))
        task.failed.connect((error) => { if (error !== "Cancelled") Router.toast(error, "error") })
    }

    Row {
        anchors.right: parent.right
        spacing: 8
        ScButton {
            visible: !root.connected
            variant: root.pastingToken ? "ghost" : "primary"
            text: root.pastingToken ? qsTr("Open again") : root.waiting ? qsTr("Cancel") : qsTr("Connect")
            onClicked: root.connect()
        }
        ScButton {
            visible: root.connected
            text: qsTr("Disconnect")
            variant: "ghost"
            onClicked: Trackers.signOut(root.service)
        }
    }

    // The token AniList shows after signing in.
    Row {
        id: tokenRow
        anchors.right: parent.right
        visible: root.service === "anilist" && root.pastingToken && !root.connected
        spacing: 8
        property var task: null
        InlineField {
            id: tokenField
            width: Theme.compact ? 170 : 220
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
                    root.pastingToken = false
                    Router.toast(qsTr("Connected to %1 as %2").arg("AniList").arg(r.name), "success")
                })
                tokenRow.task.failed.connect((error) => Router.toast(error, "error"))
            }
        }
    }
}
