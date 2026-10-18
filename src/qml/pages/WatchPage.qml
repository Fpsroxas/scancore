import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import ScanCore

// Anime player. ReAnime streams are the provider's own player page, shown in
// an embedded browser; ScanCore reads its position for resume, auto-next and
// AniSkip intro skipping.
//
// Desktop with Qt WebEngine (Linux): the video fills the page and the top
// bar and episode list float over it, fading out after a few seconds without
// mouse movement.
// System web views (phones, and Windows and macOS desktops) are native views
// drawn above everything Qt renders, so nothing can overlap them.
// Desktops dock the bar above the video and the episode list beside it; in
// full screen they hide until the pointer reaches the top edge.
// Phone portrait puts the controls and episodes below a 16:9 player;
// landscape goes full screen with a slim rail.
Rectangle {
    id: watch
    property var media: ({})
    property var episodes: []
    property int index: 0

    readonly property var episode: episodes[index] || ({})
    readonly property bool hasPrev: index > 0
    readonly property bool hasNext: index < episodes.length - 1
    readonly property var series: Anime.seriesFor(media)
    property var streamsTask: null
    property var skipTask: null
    property string currentUrl: ""
    property real position: 0
    property real duration: 0
    property real resumeAt: 0
    property bool sideOpen: false
    property bool ended: false

    readonly property bool phone: App.platform === "android"
    readonly property bool landscape: width > height
    readonly property bool fullScreen: Window.window !== null && Window.window.visibility === Window.FullScreen
    readonly property bool systemView: Anime.playerEngine === "system"
    // Desktop layout for a native player view (SCANCORE_START
    // {"playerLayout": "docked"} tries it with Qt WebEngine).
    readonly property bool docked: !phone && (systemView || App.startAction.playerLayout === "docked")
    // The embedded browser cannot decode this video (see WebPlayer).
    property bool unsupported: false
    readonly property var progress: { Library.revision; return Library.progressMap(series.id) }

    // Desktop chrome auto-hide. Docked chrome only hides in full screen, as
    // hiding it resizes the video.
    property bool idle: false
    readonly property bool chromeShown: !player.active || unsupported || (docked ? !fullScreen || !idle : !idle)

    readonly property var streams: streamsTask && streamsTask.ok ? streamsTask.result.streams : []
    readonly property var skips: skipTask && skipTask.ok ? skipTask.result : []
    readonly property var activeSkip: {
        for (const s of skips)
            if (position >= s.start && position < s.end - 1) return s
        return null
    }
    readonly property string skipLabel: activeSkip ? (activeSkip.type.indexOf("ed") >= 0 ? qsTr("Skip ending") : activeSkip.type === "recap" ? qsTr("Skip recap") : qsTr("Skip intro")) : ""
    // Settings → Anime: intros and recaps, and endings, can each be skipped
    // without asking; the Skip button is for the rest.
    function skipsItself(s) {
        if (!s) return false
        if (s.type === "ed" || s.type === "mixed-ed") return App.settings.autoSkipEndings === true
        return App.settings.autoSkip === true
    }
    readonly property bool showSkip: activeSkip !== null && !skipsItself(activeSkip)
    readonly property string episodeLine: qsTr("Episode ") + (episode.number || "") + (episode.title && !String(episode.title).startsWith("Episode") ? " · " + episode.title : "")

    color: "#000000"
    focus: true

    function load(i) {
        saveProgress(true)
        nextCountdown.stop()
        index = i
        position = 0
        duration = 0
        ended = false
        unsupported = false
        skipTask = null
        const p = Library.progressMap(series.id)[episodes[i].key]
        resumeAt = p && !p.completed && p.position ? (p.position.seconds || 0) : 0
        currentUrl = ""
        streamsTask = Anime.streams(media.anilistId, episodes[i].number)
        Presence.show("anime", series.title || "", qsTr("Episode ") + (episodes[i].number || ""))
        if (phone && !landscape) phoneList.positionViewAtIndex(i, ListView.Contain)
    }

    Component.onCompleted: {
        load(index)
        if (phone && landscape) Window.window.showFullScreen()
        wake()
        Activity.setMedia("anime")
    }
    // However the player is closed (Back, Esc, the arrow), leave full screen.
    Component.onDestruction: {
        Activity.setMedia("")
        Presence.clear()
        saveProgress(true)
        if (phone && fullScreen) Window.window.showNormal()
    }

    // Phones: rotating to landscape is the way into full screen.
    onLandscapeChanged: {
        if (!phone) return
        if (landscape) Window.window.showFullScreen()
        else Window.window.showNormal()
    }

    Connections {
        target: watch.streamsTask
        function onFinished(result) {
            // The server picked last time, when this episode has it.
            const preferred = result.streams.find(s => s.quality === App.settings.animeServer)
            if (result.streams.length > 0) watch.currentUrl = (preferred || result.streams[0]).url
        }
    }

    function pickServer(stream) {
        currentUrl = stream.url
        App.setSetting("animeServer", stream.quality)
    }

    property real lastSaved: 0
    function saveProgress(force) {
        if (!series.id || !episode.key || duration <= 0) return
        if (!force && Date.now() - lastSaved < 5000) return
        lastSaved = Date.now()
        const percent = Math.min(1, position / duration)
        Library.recordProgress(series, { key: episode.key, title: episode.title, number: String(episode.number) },
                               { type: "video", seconds: position, duration: duration }, percent, percent > 0.9)
    }

    function onPlayerTime(seconds, total, status) {
        if (total > 0 && duration === 0) {
            duration = total
            skipTask = Anime.skipTimes(media.anilistId, episode.number, Math.round(total))
            if (resumeAt > 5 && resumeAt < total - 30 && seconds < resumeAt - 5) {
                player.seek(resumeAt)
                Router.toast(qsTr("Resumed at ") + formatTime(resumeAt))
            }
        }
        if (total > 0) duration = total
        // Moving forward: the episode is playing, which counts as watching
        // time on the Activity page.
        if (seconds > position && seconds - position < 5) Activity.playing()
        position = seconds
        saveProgress(false)
        if (skipsItself(activeSkip))
            player.seek(activeSkip.end)
        if ((status === "Ended" || (total > 0 && seconds >= total - 1)) && !ended) {
            ended = true
            saveProgress(true)
            if (App.settings.autoNext !== false && hasNext) {
                nextCountdown.start()
                wake()
            }
        }
    }

    function formatTime(s) {
        s = Math.floor(s)
        const m = Math.floor(s / 60)
        return m + ":" + String(s % 60).padStart(2, "0")
    }

    function toggleFullScreen() {
        if (fullScreen) Window.window.showNormal()
        else Window.window.showFullScreen()
    }

    function wake() {
        idle = false
        idleTimer.restart()
    }

    function exit() {
        saveProgress(true)
        if (fullScreen) Window.window.showNormal()
        Router.back()
    }

    function leave() {
        if (fullScreen) Window.window.showNormal()
        else exit()
    }

    function nextEpisode() {
        if (!hasNext) return
        nextCountdown.stop()
        load(index + 1)
    }

    // The embedded browser takes keyboard focus once the video is clicked and
    // keeps keys from reaching this page, so desktop keys are shortcuts. A
    // system web view keeps them from Qt altogether; its page passes them on.
    function pageKey(press) {
        if (press.key === "Escape") leave()
        else if (Keymap.matchesWeb("player.next", press)) nextEpisode()
        else if (Keymap.matchesWeb("player.prev", press) && hasPrev) load(index - 1)
    }
    function keyHint(action) {
        const keys = Keymap.labels(action)
        return keys.length ? " (" + keys[0] + ")" : ""
    }
    // The keys come from Settings → Keyboard (see Keymap).
    Shortcut { sequence: "Esc"; enabled: !watch.phone; onActivated: watch.leave() }
    Shortcut { sequences: Keymap.sequences("player.fullscreen"); enabled: !watch.phone; onActivated: watch.toggleFullScreen() }
    Shortcut { sequences: Keymap.sequences("player.next"); enabled: !watch.phone && watch.hasNext; onActivated: watch.load(watch.index + 1) }
    Shortcut { sequences: Keymap.sequences("player.prev"); enabled: !watch.phone && watch.hasPrev; onActivated: watch.load(watch.index - 1) }

    // Any mouse movement, including over the video, brings the chrome back.
    PointerMotion {
        onMoved: watch.wake()
    }
    Timer {
        id: idleTimer
        interval: 2500
        onTriggered: {
            if (barHover.hovered || sideHover.hovered || nextCountdown.running) restart()
            else watch.idle = true
        }
    }

    // ---------------------------------------------------------------- player
    // Full screen with the docked chrome hidden: a strip along the top edge
    // that the native view does not cover, so the pointer can bring it back.
    Item {
        id: edge
        visible: watch.docked && !watch.chromeShown
        width: parent.width
        height: 4
        HoverHandler { onHoveredChanged: if (hovered) watch.wake() }
    }

    Item {
        id: stage
        x: 0
        y: watch.phone ? (watch.landscape ? 0 : bar.height)
         : watch.docked ? (watch.chromeShown ? bar.height : edge.visible ? edge.height : 0) : 0
        width: watch.phone && watch.landscape ? watch.width - rail.width
             : watch.docked ? watch.width - side.width : watch.width
        height: watch.phone && !watch.landscape ? Math.round(Math.min(watch.width * 9 / 16, watch.height * 0.45))
              : watch.height - y

        Loader {
            id: player
            anchors.fill: parent
            // A system web view draws above everything in the window, so it
            // steps aside while the comments panel is open.
            visible: !(watch.systemView && comments.visible)
            active: Anime.playerEngine !== "" && watch.currentUrl.length > 0
            source: watch.systemView ? "../components/WebPlayerSystem.qml" : "../components/WebPlayer.qml"
            onLoaded: item.url = Qt.binding(() => watch.currentUrl)
            function seek(seconds) { if (item) item.seek(seconds) }
            Connections {
                target: player.item
                function onTime(seconds, total, status) { watch.onPlayerTime(seconds, total, status) }
                function onInteracted() { watch.wake() }
                function onKey(press) { watch.pageKey(press) }
                function onUnsupported() { watch.unsupported = true }
            }
        }

        // This build's browser cannot play the video: say so, and offer the
        // system browser, which can. Drawn over the (Qt WebEngine) player.
        Rectangle {
            anchors.fill: parent
            visible: watch.unsupported && player.active
            color: "#000000"
            Column {
                anchors.centerIn: parent
                width: Math.min(460, parent.width - 48)
                spacing: 14
                Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    name: "tv-minimal"
                    size: 34
                    color: "#a1a1aa"
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: qsTr("This copy of SCANCORE cannot play this video")
                    color: "white"
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.textLg
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: qsTr("Its built-in browser lacks the H.264 video codec. Watch this episode in your browser, or use ")
                        + qsTr("the SCANCORE Flatpak or your distribution's package, which include it.")
                    color: "#a1a1aa"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    lineHeight: 1.3
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    ScButton {
                        variant: "primary"
                        text: qsTr("Open in browser")
                        iconName: "external-link"
                        onClicked: App.openUrl(watch.currentUrl)
                    }
                    ScButton {
                        text: qsTr("Get SCANCORE")
                        onClicked: App.openUrl("https://scancore.onl/desktop")
                    }
                }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 14
            visible: !player.active
            Spinner {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: watch.streamsTask !== null && watch.streamsTask.loading
                size: 30
                color: "#a1a1aa"
            }
            Text {
                width: Math.min(440, stage.width - 32)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: text.length > 0
                text: Anime.playerEngine === "" ? qsTr("This build of SCANCORE has no embedded browser, which the ReAnime player needs.")
                    : watch.streamsTask && !watch.streamsTask.loading
                      ? (watch.streamsTask.ok ? (watch.streamsTask.result.message || "") : watch.streamsTask.error) : ""
                color: "#d4d4d8"
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
            }
            ScButton {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: watch.streamsTask !== null && !watch.streamsTask.loading && watch.streams.length === 0
                text: qsTr("Try again")
                iconName: "refresh-cw"
                onClicked: watch.load(watch.index)
            }
        }

        // Desktop only: on phones these would sit under the native WebView.
        ScButton {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 40
            anchors.bottomMargin: 80
            visible: !watch.phone && !watch.systemView && watch.showSkip
            variant: "primary"
            iconName: "skip-forward"
            text: watch.skipLabel
            onClicked: player.seek(watch.activeSkip.end)
        }

        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 40
            width: 320
            height: 96
            radius: Theme.radiusLg
            color: "#141416"
            border.width: 1
            border.color: "#2a2a30"
            visible: !watch.phone && !watch.systemView && nextCountdown.running
            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10
                Text {
                    text: qsTr("Next episode in ") + Math.ceil(nextCountdown.remaining) + "s"
                    color: "white"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                }
                Row {
                    spacing: 8
                    ScButton { compact: true; variant: "primary"; text: qsTr("Play now"); onClicked: watch.nextEpisode() }
                    ScButton { compact: true; text: qsTr("Cancel"); onClicked: nextCountdown.stop() }
                }
            }
        }
        Timer {
            id: nextCountdown
            // Settings → Anime: how long the countdown lasts.
            readonly property real length: Number(App.settings.nextCountdown) || 8
            property real remaining: length
            interval: 100
            repeat: true
            onRunningChanged: if (running) remaining = length
            onTriggered: {
                remaining -= 0.1
                if (remaining <= 0) {
                    stop()
                    if (watch.hasNext) watch.load(watch.index + 1)
                }
            }
        }
    }

    // ---------------------------------------------------------------- top bar
    // Desktop: floats over the video and fades with the rest of the chrome.
    // Phone portrait: a plain header above the player. Hidden in landscape.
    Rectangle {
        id: bar
        width: parent.width
        height: watch.phone ? 52 : watch.docked ? 64 : 72
        visible: watch.phone ? !watch.landscape : watch.docked ? watch.chromeShown : opacity > 0.01
        opacity: watch.phone || watch.docked || watch.chromeShown ? 1 : 0
        Behavior on opacity { enabled: !watch.phone && !watch.docked; NumberAnimation { duration: Motion.slow } }
        color: watch.phone || watch.docked ? "#0b0b0c" : "transparent"
        gradient: watch.phone || watch.docked ? null : barShade
        Gradient {
            id: barShade
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.85) }
            GradientStop { position: 1.0; color: "transparent" }
        }
        HoverHandler { id: barHover }

        IconButton {
            id: backButton
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            iconName: "arrow-left"
            iconColor: "white"
            tip: qsTr("Close player (Esc)")
            onClicked: watch.exit()
        }
        Column {
            anchors.left: backButton.right
            anchors.leftMargin: 10
            anchors.right: watch.phone ? parent.right : barActions.left
            anchors.rightMargin: watch.phone ? 16 : 12
            anchors.verticalCenter: parent.verticalCenter
            Text {
                width: parent.width
                text: watch.media.title || ""
                color: "white"
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                visible: !watch.phone
                text: watch.episodeLine
                color: "#a1a1aa"
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                elide: Text.ElideRight
            }
        }
        // Docked: skip and up-next live here, as nothing can float over the video.
        Row {
            id: barActions
            anchors.right: controls.left
            anchors.rightMargin: visible ? 12 : 0
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            visible: watch.systemView && !watch.phone && (skipNow.visible || nextNow.visible)
            ScButton {
                id: skipNow
                visible: watch.showSkip
                compact: true
                variant: "primary"
                iconName: "skip-forward"
                text: watch.skipLabel
                onClicked: player.seek(watch.activeSkip.end)
            }
            ScButton {
                id: nextNow
                visible: nextCountdown.running
                compact: true
                variant: "primary"
                text: qsTr("Next in ") + Math.ceil(nextCountdown.remaining) + "s"
                onClicked: watch.nextEpisode()
            }
            ScButton {
                visible: nextCountdown.running
                compact: true
                text: qsTr("Cancel")
                onClicked: nextCountdown.stop()
            }
        }
        Row {
            id: controls
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            visible: !watch.phone
            Repeater {
                model: watch.streams
                Chip {
                    required property var modelData
                    text: modelData.quality
                    selected: watch.currentUrl === modelData.url
                    onClicked: watch.pickServer(modelData)
                }
            }
            Item { width: 8; height: 1 }
            IconButton { iconName: "skip-back"; iconColor: "white"; enabled: watch.hasPrev; tip: qsTr("Previous episode") + watch.keyHint("player.prev"); onClicked: watch.load(watch.index - 1) }
            IconButton { iconName: "skip-forward"; iconColor: "white"; enabled: watch.hasNext; tip: qsTr("Next episode") + watch.keyHint("player.next"); onClicked: watch.load(watch.index + 1) }
            IconButton { iconName: "list"; iconColor: "white"; active: watch.sideOpen; tip: qsTr("Episodes"); onClicked: watch.sideOpen = !watch.sideOpen }
            IconButton { iconName: "message-circle"; iconColor: "white"; tip: qsTr("Comments"); onClicked: comments.open() }
            IconButton {
                iconName: "external-link"
                iconColor: "white"
                enabled: watch.currentUrl.length > 0
                tip: qsTr("Open in your browser")
                onClicked: App.openUrl(watch.currentUrl)
            }
            IconButton {
                iconName: watch.fullScreen ? "minimize" : "maximize"
                iconColor: "white"
                tip: (watch.fullScreen ? qsTr("Exit full screen") : qsTr("Full screen")) + watch.keyHint("player.fullscreen")
                onClicked: watch.toggleFullScreen()
            }
        }
    }

    // ---------------------------------------------------------------- episodes
    component EpisodeRow: AbstractButton {
        id: row
        required property var modelData
        required property int index
        readonly property var state: watch.progress[modelData.key] || null
        height: 64
        hoverEnabled: true
        onClicked: watch.load(index)
        background: Rectangle {
            radius: Theme.radiusMd
            color: row.index === watch.index ? "#222226" : row.hovered ? "#18181b" : "transparent"
        }
        contentItem: Row {
            spacing: 10
            leftPadding: 6
            Item {
                width: 92
                height: 52
                anchors.verticalCenter: parent.verticalCenter
                Cover { anchors.fill: parent; radius: 6; source: row.modelData.image || ""; title: qsTr("EP ") + row.modelData.number }
                Rectangle {
                    visible: row.state !== null
                    anchors.bottom: parent.bottom
                    width: parent.width * (row.state ? (row.state.completed ? 1 : row.state.percent) : 0)
                    height: 3
                    color: "white"
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: row.width - 92 - 10 - 6 - 8
                Text {
                    width: parent.width
                    text: qsTr("Episode ") + row.modelData.number
                    color: row.index === watch.index ? "white" : "#d4d4d8"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    text: row.modelData.title
                    color: "#8b8b93"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs + 1
                    elide: Text.ElideRight
                }
            }
        }
    }

    // Desktop: slides over the right edge of the video; docked, it sits
    // beside it (and does not animate, which would resize the video on
    // every frame).
    Rectangle {
        id: side
        visible: !watch.phone && width > 0
        y: bar.visible ? bar.height : 0
        height: parent.height - y
        anchors.right: parent.right
        width: watch.sideOpen && watch.chromeShown ? 340 : 0
        color: watch.docked ? "#0b0b0c" : Qt.rgba(0.055, 0.055, 0.063, 0.94)
        radius: watch.docked ? 0 : Theme.radiusLg
        clip: true
        Behavior on width { enabled: !watch.docked; NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
        HoverHandler { id: sideHover }

        ListView {
            anchors.fill: parent
            anchors.margins: 8
            width: 324
            clip: true
            model: watch.phone ? [] : watch.episodes
            currentIndex: watch.index
            ScrollBar.vertical: ScScrollBar {}
            Component.onCompleted: positionViewAtIndex(watch.index, ListView.Center)
            delegate: EpisodeRow { width: 324 }
        }
    }

    // Phone portrait: details, skip/next and the episode list under the player.
    Item {
        id: phonePanel
        visible: watch.phone && !watch.landscape
        anchors.top: stage.bottom
        anchors.bottom: parent.bottom
        width: parent.width

        Column {
            id: phoneHead
            width: parent.width
            topPadding: 14
            bottomPadding: 10
            spacing: 12

            Text {
                x: 16
                width: parent.width - 32
                text: watch.episodeLine
                color: "white"
                font.family: Theme.fontSans
                font.pixelSize: Theme.textMd
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Item {
                x: 16
                width: parent.width - 32
                height: 36
                Flickable {
                    anchors.left: parent.left
                    anchors.right: phoneNav.left
                    anchors.rightMargin: 8
                    height: parent.height
                    contentWidth: chips.width
                    clip: true
                    flickableDirection: Flickable.HorizontalFlick
                    Row {
                        id: chips
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter
                        Repeater {
                            model: watch.streams
                            Chip {
                                required property var modelData
                                text: modelData.quality
                                selected: watch.currentUrl === modelData.url
                                onClicked: watch.pickServer(modelData)
                            }
                        }
                    }
                }
                Row {
                    id: phoneNav
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    IconButton { iconName: "skip-back"; iconColor: "white"; diameter: 40; enabled: watch.hasPrev; onClicked: watch.load(watch.index - 1) }
                    IconButton { iconName: "skip-forward"; iconColor: "white"; diameter: 40; enabled: watch.hasNext; onClicked: watch.load(watch.index + 1) }
                }
            }
            Row {
                x: 16
                spacing: 8
                visible: (watch.showSkip) || nextCountdown.running
                ScButton {
                    visible: watch.showSkip
                    variant: "primary"
                    compact: true
                    iconName: "skip-forward"
                    text: watch.skipLabel
                    onClicked: player.seek(watch.activeSkip.end)
                }
                ScButton {
                    visible: nextCountdown.running
                    variant: "primary"
                    compact: true
                    text: qsTr("Next in ") + Math.ceil(nextCountdown.remaining) + "s"
                    onClicked: watch.nextEpisode()
                }
                ScButton {
                    visible: nextCountdown.running
                    compact: true
                    text: qsTr("Cancel")
                    onClicked: nextCountdown.stop()
                }
            }
            Rectangle { width: parent.width; height: 1; color: "#1d1d21" }
        }

        ListView {
            id: phoneList
            anchors.top: phoneHead.bottom
            anchors.bottom: parent.bottom
            x: 8
            width: parent.width - 16
            clip: true
            model: watch.phone ? watch.episodes : []
            currentIndex: watch.index
            Component.onCompleted: positionViewAtIndex(watch.index, ListView.Contain)
            delegate: EpisodeRow { width: phoneList.width }
        }
    }

    // Phone landscape: the player is full screen; a thin rail keeps the
    // essentials reachable without covering the video. It hides with the
    // rest of the chrome and comes back when the video is tapped.
    Rectangle {
        id: rail
        visible: watch.phone && watch.landscape && watch.chromeShown
        width: visible ? 56 : 0
        height: parent.height
        anchors.right: parent.right
        color: "#0b0b0c"
        Column {
            anchors.centerIn: parent
            spacing: 14
            IconButton { iconName: "arrow-left"; iconColor: "white"; diameter: 44; onClicked: watch.exit() }
            IconButton {
                iconName: "chevron-right"
                iconColor: "white"
                diameter: 44
                filled: true
                visible: watch.showSkip
                onClicked: player.seek(watch.activeSkip.end)
            }
            IconButton { iconName: "skip-forward"; iconColor: "white"; diameter: 44; enabled: watch.hasNext; onClicked: watch.nextEpisode() }
        }
    }

    CommentsPanel {
        id: comments
        media: "anime"
        anilistId: watch.media.anilistId || 0
        title: watch.media.title || watch.series.title || ""
        unit: String(watch.episode.number || "")
        label: qsTr("Episode ") + (watch.episode.number || "")
    }
}
