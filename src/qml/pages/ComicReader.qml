import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import ScanCore

// Full-window comic reader. Vertical strip (webtoons) or single pages
// (manga), with eased scrolling, auto-hiding controls, progress saved as you
// read and the next chapter fetched ahead of time.
Rectangle {
    id: reader
    property var series: ({})
    property var chapters: []
    property int index: 0

    readonly property var unit: chapters[index] || ({})
    readonly property bool hasPrev: index > 0
    readonly property bool hasNext: index < chapters.length - 1

    property var task: null
    property var nextTask: null
    readonly property var panels: task && task.ok ? task.result.panels : []
    readonly property string referer: task && task.ok ? task.result.referer : ""

    readonly property string mode: App.settings.readerMode === "paged" ? "paged" : "strip"
    readonly property int stripWidth: Math.min(width, App.settings.readerWidth || 820)
    property int currentPanel: 0
    property bool controlsShown: true
    property bool finished: false
    property var resumePosition: null

    // Drawn under the phone's status and navigation bars; the controls are
    // padded by these insets instead (see the immersive stack in Main.qml).
    readonly property bool edgeToEdge: true
    readonly property real insetTop: SafeArea.margins.top
    readonly property real insetBottom: SafeArea.margins.bottom

    color: "#000000"
    focus: true

    function load(i, fromEnd) {
        saveProgress(true)
        index = i
        finished = false
        currentPanel = 0
        const last = Library.progressMap(series.id)[chapters[i].key]
        resumePosition = last && !last.completed && last.position ? last.position : null
        if (nextTask && nextTask.ok && nextTask.done && nextFor === i) task = nextTask
        else task = Comics.panels(chapters[i].url, series.id, chapters[i].key)
        nextTask = null
        strip.contentY = strip.originY
        pageView.page = 0
        showControls()
    }
    property int nextFor: -1

    Component.onCompleted: load(index)
    Component.onDestruction: saveProgress(true)

    Connections {
        target: reader.task
        function onFinished() {
            Qt.callLater(() => {
                if (reader.resumePosition && reader.resumePosition.panel > 0) {
                    const p = Math.min(reader.resumePosition.panel, reader.panels.length - 1)
                    if (reader.mode === "strip") strip.positionViewAtIndex(p, ListView.Beginning)
                    else pageView.page = p
                    reader.currentPanel = p
                }
            })
        }
        function onFailed(error) { reader.showControls() }
    }

    // ------------------------------------------------------------ progress

    property real lastSaved: 0
    function saveProgress(force) {
        if (!series.id || !unit.key || panels.length === 0) return
        const now = Date.now()
        if (!force && now - lastSaved < 1500) return
        lastSaved = now
        const percent = finished ? 1 : (currentPanel + 1) / panels.length
        Library.recordProgress(series, unit, { type: reader.mode, panel: currentPanel }, percent, finished || percent >= 0.98)
    }

    onCurrentPanelChanged: {
        saveProgress(false)
        if (panels.length > 0 && currentPanel >= panels.length * 0.7) prefetchNext()
    }

    function prefetchNext() {
        if (!hasNext || nextFor === index + 1) return
        nextFor = index + 1
        nextTask = Comics.panels(chapters[index + 1].url, series.id, chapters[index + 1].key)
    }
    Connections {
        target: reader.nextTask
        function onFinished(result) { Comics.prefetch(result.panels, result.referer, 6) }
    }

    function markFinished() {
        if (finished) return
        finished = true
        saveProgress(true)
    }

    // ------------------------------------------------------------ controls

    function showControls() {
        controlsShown = true
        hideTimer.restart()
    }
    function toggleControls() {
        if (controlsShown) controlsShown = false
        else showControls()
    }
    Timer {
        id: hideTimer
        interval: 2600
        onTriggered: {
            // Stay up while the pointer rests on the controls, a popup is
            // open, or there are no pages yet (loading, or an error to act on).
            if (settingsPopup.opened || chapterPopup.opened || scrubber.pressed || topHover.hovered || bottomHover.hovered
                || reader.panels.length === 0)
                restart()
            else
                reader.controlsShown = false
        }
    }

    function scrollBy(delta) {
        if (mode === "strip") {
            const base = scrollAnim.running ? scrollAnim.to : strip.contentY
            const target = Math.max(strip.originY, Math.min(strip.contentHeight + strip.originY - strip.height, base + delta))
            scrollAnim.to = target
            scrollAnim.restart()
        } else {
            pageView.turn(delta > 0 ? 1 : -1)
        }
    }

    function exit() {
        saveProgress(true)
        Router.back()
    }

    function keyHint(action) {
        const keys = Keymap.labels(action)
        return keys.length ? " (" + keys[0] + ")" : ""
    }

    // Scrolling keys come from Settings → Keyboard too, but only count
    // while the reader has focus (not in its chapter list, say).
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape) exit()
        else if (Keymap.matches("reader.down", event)) scrollBy(height * 0.18)
        else if (Keymap.matches("reader.up", event)) scrollBy(-height * 0.18)
        else if (Keymap.matches("reader.pageDown", event)) scrollBy(height * 0.85)
        else if (Keymap.matches("reader.pageUp", event)) scrollBy(-height * 0.85)
        else if (event.key === Qt.Key_Right) mode === "paged" ? pageView.turn(App.settings.readerDirection === "rtl" ? -1 : 1) : (hasNext && load(index + 1))
        else if (event.key === Qt.Key_Left) mode === "paged" ? pageView.turn(App.settings.readerDirection === "rtl" ? 1 : -1) : (hasPrev && load(index - 1))
        else return
        event.accepted = true
    }

    // The rest come from Settings → Keyboard (see Keymap).
    Shortcut { sequences: Keymap.sequences("reader.next"); onActivated: if (reader.hasNext) reader.load(reader.index + 1) }
    Shortcut { sequences: Keymap.sequences("reader.prev"); onActivated: if (reader.hasPrev) reader.load(reader.index - 1) }
    Shortcut {
        sequences: Keymap.sequences("reader.first")
        onActivated: reader.mode === "strip" ? strip.positionViewAtBeginning() : (pageView.page = 0)
    }
    Shortcut {
        sequences: Keymap.sequences("reader.last")
        onActivated: reader.mode === "strip" ? strip.positionViewAtEnd() : (pageView.page = reader.panels.length - 1)
    }
    Shortcut {
        sequences: Keymap.sequences("reader.fullscreen")
        onActivated: Window.window.visibility === Window.FullScreen ? Window.window.showNormal() : Window.window.showFullScreen()
    }
    Shortcut { sequences: Keymap.sequences("reader.mode"); onActivated: App.setSetting("readerMode", reader.mode === "strip" ? "paged" : "strip") }
    Shortcut { sequences: Keymap.sequences("reader.wider"); onActivated: App.setSetting("readerWidth", Math.min(2400, reader.stripWidth + 80)) }
    Shortcut { sequences: Keymap.sequences("reader.narrower"); onActivated: App.setSetting("readerWidth", Math.max(360, reader.stripWidth - 80)) }

    // ------------------------------------------------------------ strip

    ListView {
        id: strip
        anchors.fill: parent
        visible: reader.mode === "strip"
        model: reader.mode === "strip" ? reader.panels : []
        cacheBuffer: Math.max(2400, height * 3)
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 7000
        flickDeceleration: 2600
        spacing: App.settings.readerGap || 0
        reuseItems: false
        ScrollBar.vertical: ScScrollBar { policy: reader.controlsShown ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded }

        onContentYChanged: {
            const i = indexAt(width / 2, contentY + height * 0.35)
            if (i >= 0) reader.currentPanel = i
            if (atYEnd && reader.panels.length > 0) reader.markFinished()
            if (moving || scrollAnim.running) reader.controlsShown = false
        }

        NumberAnimation {
            id: scrollAnim
            target: strip
            property: "contentY"
            duration: 300
            easing.type: Easing.OutCubic
        }

        // Phone: a tap toggles the controls; on desktop single clicks are left
        // alone and a double click does. On the list itself, which takes
        // touches before anything behind it.
        TapHandler {
            onSingleTapped: if (App.platform === "android") reader.toggleControls()
            onDoubleTapped: if (App.platform !== "android") reader.toggleControls()
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse
            onWheel: (event) => {
                if (event.modifiers & Qt.ControlModifier) {
                    App.setSetting("readerWidth", Math.max(360, Math.min(2400, reader.stripWidth + (event.angleDelta.y > 0 ? 60 : -60))))
                    return
                }
                reader.scrollBy(-event.angleDelta.y / 120 * 180)
            }
        }

        delegate: Item {
            id: panelItem
            required property var modelData
            required property int index
            width: strip.width
            height: image.status === PanelImage.Ready && image.sourceWidth > 0
                ? reader.stripWidth * image.sourceHeight / image.sourceWidth
                : (modelData.width > 0 && modelData.height > 0 ? reader.stripWidth * modelData.height / modelData.width : reader.stripWidth * 1.4)

            PanelImage {
                id: image
                anchors.horizontalCenter: parent.horizontalCenter
                width: reader.stripWidth
                height: parent.height
                source: panelItem.modelData.url
                referer: reader.referer
                maxDecodeWidth: Math.ceil(Math.min(2400, reader.stripWidth * Screen.devicePixelRatio))
                opacity: status === PanelImage.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Motion.normal } }
            }
            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: image.status !== PanelImage.Ready
                Spinner {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: image.status === PanelImage.Loading
                    color: "#7a7a80"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: image.status === PanelImage.Error
                    text: "Page " + (panelItem.index + 1) + " did not load"
                    color: "#a0a0a8"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                }
                ScButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: image.status === PanelImage.Error
                    text: "Retry"
                    compact: true
                    onClicked: image.reload()
                }
            }
        }

        footer: Item {
            width: strip.width
            height: Math.max(320, strip.height * 0.6)
            visible: reader.panels.length > 0
            NextChapterCard {
                anchors.centerIn: parent
                host: reader
            }
        }
    }

    // ------------------------------------------------------------ paged

    Item {
        id: pageView
        anchors.fill: parent
        visible: reader.mode === "paged"
        property int page: 0
        property int direction: 1
        readonly property bool rtl: App.settings.readerDirection === "rtl"

        function turn(step) {
            const next = page + step
            if (next < 0) {
                if (reader.hasPrev) reader.load(reader.index - 1)
                return
            }
            if (next >= reader.panels.length) {
                reader.markFinished()
                if (reader.hasNext) reader.load(reader.index + 1)
                return
            }
            direction = step
            page = next
        }
        onPageChanged: {
            reader.currentPanel = page
            if (page === reader.panels.length - 1) reader.markFinished()
            pageSwap.restart()
        }

        Item {
            id: pageHolder
            anchors.fill: parent
            PanelImage {
                id: pageImage
                readonly property real ratio: sourceWidth > 0 ? sourceHeight / sourceWidth : 1.45
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height / ratio)
                height: width * ratio
                source: reader.mode === "paged" && reader.panels.length > 0 ? reader.panels[Math.min(pageView.page, reader.panels.length - 1)].url : ""
                referer: reader.referer
                maxDecodeWidth: 2400
            }
            Spinner {
                anchors.centerIn: parent
                visible: pageImage.status === PanelImage.Loading
                color: "#7a7a80"
            }
            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: pageImage.status === PanelImage.Error
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Page " + (pageView.page + 1) + " did not load"
                    color: "#a0a0a8"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                }
                ScButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Retry"
                    compact: true
                    onClicked: pageImage.reload()
                }
            }
        }
        // Preload neighbours.
        Repeater {
            model: reader.mode === "paged" ? [pageView.page + 1, pageView.page + 2, pageView.page - 1] : []
            PanelImage {
                required property var modelData
                visible: false
                width: 1
                height: 1
                source: modelData >= 0 && modelData < reader.panels.length ? reader.panels[modelData].url : ""
                referer: reader.referer
                maxDecodeWidth: 2400
            }
        }
        ParallelAnimation {
            id: pageSwap
            NumberAnimation { target: pageHolder; property: "opacity"; from: 0.35; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { target: pageHolder; property: "x"; from: 26 * pageView.direction * (pageView.rtl ? -1 : 1); to: 0; duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.decelerate }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: (mouse) => {
                const third = width / 3
                if (mouse.x < third) pageView.turn(pageView.rtl ? 1 : -1)
                else if (mouse.x > third * 2) pageView.turn(pageView.rtl ? -1 : 1)
                else reader.controlsShown ? (reader.controlsShown = false) : reader.showControls()
            }
            onWheel: (wheel) => pageView.turn(wheel.angleDelta.y < 0 ? 1 : -1)

            // Swipe (or drag) sideways to turn the page; the page follows
            // and springs back from a short swipe. Inside the MouseArea, so it
            // sees the press before the MouseArea takes it.
            DragHandler {
                id: swipe
                target: null
                yAxis.enabled: false
                onTranslationChanged: if (active) pageHolder.x = translation.x * 0.7
                onActiveChanged: {
                    if (active) {
                        pageSwap.stop()
                        return
                    }
                    const dx = translation.x
                    if (Math.abs(dx) > Math.min(90, pageView.width * 0.12)) {
                        const forward = dx < 0 ? !pageView.rtl : pageView.rtl
                        pageView.turn(forward ? 1 : -1)
                    }
                    if (!pageSwap.running) springBack.start()
                }
            }
            NumberAnimation { id: springBack; target: pageHolder; property: "x"; to: 0; duration: Motion.normal; easing.type: Easing.OutCubic }
        }
    }

    // ------------------------------------------------------------ states

    Column {
        anchors.centerIn: parent
        spacing: 14
        visible: reader.task !== null && (reader.task.loading || !reader.task.ok)
        Spinner {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: reader.task !== null && reader.task.loading
            size: 30
            color: "#9a9aa2"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: reader.task !== null && reader.task.loading
            text: reader.unit.title || ""
            color: "#9a9aa2"
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
        }
        Text {
            width: Math.min(420, reader.width - 48)
            visible: reader.task !== null && !reader.task.loading && !reader.task.ok
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: reader.task ? reader.task.error : ""
            color: "#d4d4d8"
            font.family: Theme.fontSans
            font.pixelSize: Theme.textMd
        }
        ScButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: reader.task !== null && !reader.task.loading && !reader.task.ok
            text: "Try again"
            iconName: "refresh-cw"
            onClicked: reader.load(reader.index)
        }
    }

    // Desktop: moving the mouse reveals the controls.
    PointerMotion {
        onMoved: reader.showControls()
    }

    // Thin progress rail on the right edge.
    Rectangle {
        visible: App.settings.readerProgressBar !== false && reader.panels.length > 0
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        color: Qt.rgba(1, 1, 1, 0.06)
        Rectangle {
            width: parent.width
            height: parent.height * (reader.finished ? 1 : (reader.currentPanel + 1) / Math.max(1, reader.panels.length))
            color: Qt.rgba(1, 1, 1, 0.5)
            Behavior on height { NumberAnimation { duration: Motion.normal; easing.type: Easing.OutCubic } }
        }
    }

    // ------------------------------------------------------------ top bar

    // Keeps the status bar legible over light pages while the controls are hidden.
    Rectangle {
        width: parent.width
        height: reader.insetTop
        visible: height > 0
        color: Qt.rgba(0, 0, 0, 0.6)
    }

    Item {
        id: topBar
        width: parent.width
        height: 76 + reader.insetTop
        y: reader.controlsShown ? 0 : -height
        opacity: reader.controlsShown ? 1 : 0
        Behavior on y { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }
        HoverHandler { id: topHover; enabled: App.platform !== "android" }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.85) }
                GradientStop { position: reader.insetTop / topBar.height; color: Qt.rgba(0, 0, 0, 0.8) }
                GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0) }
            }
        }
        Row {
            x: 18
            y: 16 + reader.insetTop
            spacing: 12
            IconButton {
                iconName: "arrow-left"
                iconColor: "white"
                tip: "Close reader (Esc)"
                onClicked: reader.exit()
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1
                Text {
                    text: reader.series.title || ""
                    color: "white"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textMd
                    font.weight: Font.DemiBold
                    width: Math.min(implicitWidth, reader.width - 100 - topActions.width)
                    elide: Text.ElideRight
                }
                Text {
                    text: (reader.unit.title || "") + (reader.task && reader.task.ok && reader.task.result.offline ? "  ·  Downloaded" : "")
                    color: "#b4b4bb"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    width: Math.min(implicitWidth, reader.width - 100 - topActions.width)
                    elide: Text.ElideRight
                }
            }
        }
        Row {
            id: topActions
            anchors.right: parent.right
            anchors.rightMargin: 18
            y: 16 + reader.insetTop
            spacing: 4
            IconButton {
                iconName: "list"
                iconColor: "white"
                tip: "Chapters"
                onClicked: chapterPopup.open()
            }
            IconButton {
                iconName: reader.mode === "strip" ? "move-vertical" : "layers"
                iconColor: "white"
                tip: (reader.mode === "strip" ? "Switch to pages" : "Switch to vertical strip") + reader.keyHint("reader.mode")
                onClicked: App.setSetting("readerMode", reader.mode === "strip" ? "paged" : "strip")
            }
            IconButton {
                iconName: "sliders-horizontal"
                iconColor: "white"
                tip: "Reader settings"
                onClicked: settingsPopup.open()
            }
            IconButton {
                iconName: Window.window && Window.window.visibility === Window.FullScreen ? "minimize" : "maximize"
                iconColor: "white"
                tip: "Full screen" + reader.keyHint("reader.fullscreen")
                onClicked: Window.window.visibility === Window.FullScreen ? Window.window.showNormal() : Window.window.showFullScreen()
            }
        }
    }

    // ------------------------------------------------------------ bottom bar

    Item {
        id: bottomBar
        width: parent.width
        height: 84 + reader.insetBottom
        y: reader.controlsShown ? parent.height - height : parent.height
        opacity: reader.controlsShown ? 1 : 0
        Behavior on y { NumberAnimation { duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.emphasized } }
        Behavior on opacity { NumberAnimation { duration: Motion.normal } }
        HoverHandler { id: bottomHover; enabled: App.platform !== "android" }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0) }
                GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.85) }
            }
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18 + reader.insetBottom
            width: Math.min(parent.width - 40, 720)
            height: 48
            radius: 24
            color: Qt.rgba(0.08, 0.08, 0.09, 0.92)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.08)

            IconButton {
                id: prevButton
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                iconName: "skip-back"
                iconColor: "white"
                enabled: reader.hasPrev
                tip: "Previous chapter" + reader.keyHint("reader.prev")
                onClicked: reader.load(reader.index - 1)
            }
            Text {
                id: counter
                anchors.left: prevButton.right
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 64
                text: reader.panels.length ? (reader.currentPanel + 1) + " / " + reader.panels.length : ""
                color: "#d4d4d8"
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                font.features: { "tnum": 1 }
            }
            ScSlider {
                id: scrubber
                anchors.left: counter.right
                anchors.right: nextButton.left
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                from: 0
                to: Math.max(1, reader.panels.length - 1)
                stepSize: 1
                snapMode: Slider.SnapAlways
                value: reader.currentPanel
                onMoved: {
                    if (reader.mode === "strip") strip.positionViewAtIndex(value, ListView.Beginning)
                    else pageView.page = value
                }
            }
            IconButton {
                id: nextButton
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                iconName: "skip-forward"
                iconColor: "white"
                enabled: reader.hasNext
                tip: "Next chapter" + reader.keyHint("reader.next")
                onClicked: reader.load(reader.index + 1)
            }
        }
    }

    // ------------------------------------------------------------ popups

    Popup {
        id: chapterPopup
        x: parent.width - width - 18
        y: 70 + reader.insetTop
        width: Math.min(340, parent.width - 36)
        height: Math.min(parent.height - 160 - reader.insetTop - reader.insetBottom, 560)
        padding: 6
        onOpened: chapterList.positionViewAtIndex(reader.index, ListView.Center)
        background: Rectangle { radius: Theme.radiusLg; color: "#151517"; border.width: 1; border.color: "#2a2a30" }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast } }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast } }
        contentItem: ListView {
            id: chapterList
            clip: true
            model: reader.chapters
            ScrollBar.vertical: ScScrollBar {}
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                width: chapterList.width
                height: 38
                onClicked: {
                    chapterPopup.close()
                    reader.load(index)
                }
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: index === reader.index ? "#2a2a30" : parent.hovered ? "#1f1f23" : "transparent"
                }
                contentItem: Text {
                    text: modelData.title
                    color: index === reader.index ? "white" : "#c4c4cb"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    font.weight: index === reader.index ? Font.DemiBold : Font.Normal
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }
    }

    Popup {
        id: settingsPopup
        x: parent.width - width - 18
        y: 70 + reader.insetTop
        width: Math.min(320, parent.width - 36)
        padding: 18
        background: Rectangle { radius: Theme.radiusLg; color: "#151517"; border.width: 1; border.color: "#2a2a30" }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast }
                NumberAnimation { property: "y"; from: 60 + reader.insetTop; to: 70 + reader.insetTop; duration: Motion.normal; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast } }
        contentItem: Column {
            spacing: 16
            Text { text: "Layout"; color: "#9a9aa2"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            Segmented {
                options: [{ value: "strip", label: "Vertical", icon: "move-vertical" }, { value: "paged", label: "Pages", icon: "layers" }]
                value: reader.mode
                onPicked: (v) => App.setSetting("readerMode", v)
            }
            Segmented {
                visible: reader.mode === "paged"
                options: [{ value: "ltr", label: "Left to right" }, { value: "rtl", label: "Right to left" }]
                value: App.settings.readerDirection || "ltr"
                onPicked: (v) => App.setSetting("readerDirection", v)
            }
            Text { visible: reader.mode === "strip"; text: "Width · " + reader.stripWidth + " px"; color: "#9a9aa2"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            ScSlider {
                visible: reader.mode === "strip"
                width: parent.width
                from: 360
                to: 2000
                stepSize: 20
                value: reader.stripWidth
                onMoved: App.setSetting("readerWidth", value)
            }
            Text { visible: reader.mode === "strip"; text: "Gap between panels · " + (App.settings.readerGap || 0) + " px"; color: "#9a9aa2"; font.family: Theme.fontSans; font.pixelSize: Theme.textXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
            ScSlider {
                visible: reader.mode === "strip"
                width: parent.width
                from: 0
                to: 40
                stepSize: 2
                value: App.settings.readerGap || 0
                onMoved: App.setSetting("readerGap", value)
            }
            Row {
                spacing: 10
                Toggle {
                    checked: App.settings.readerProgressBar !== false
                    onToggled: App.setSetting("readerProgressBar", checked)
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Progress rail"; color: "#d4d4d8"; font.family: Theme.fontSans; font.pixelSize: Theme.textSm }
            }
        }
    }
}
