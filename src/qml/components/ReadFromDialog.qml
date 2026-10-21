import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Opens a title from the chosen tracker's catalogue: finds it on the sources
// (under any of its names) and opens it there. It opens straight away when
// exactly one source in the content language has it, and otherwise asks,
// content language first.
Dialog {
    id: dialog
    property var item: ({})
    property var task: null
    readonly property bool novel: item.media === "novel"
    readonly property var matches: task && task.ok ? task.result.matches : []
    // Another language to look in (comics), when the user's own has nothing.
    property string language: ""
    property bool choosing: false
    // Looking only in installed extensions (a title found on a built-in
    // source, to read it elsewhere).
    property bool extensions: false
    readonly property bool hasExtensions: novel ? Novels.hasExtensions() : Comics.hasExtensions()
    readonly property var otherLanguages: novel ? [] : Comics.otherLanguages().filter(l => l.value !== language)
    readonly property bool showLanguages: !novel && !extensions && !!task && task.ok && (matches.length === 0 || choosing) && otherLanguages.length > 0

    function start(entry, extensionsOnly) {
        item = entry
        language = ""
        choosing = false
        extensions = extensionsOnly === true
        task = entry.media === "novel" ? Novels.resolve(entry, extensions) : Comics.resolve(entry, "", extensions)
        open()
    }

    function lookInExtensions() {
        extensions = true
        language = ""
        choosing = false
        task = novel ? Novels.resolve(item, true) : Comics.resolve(item, "", true)
    }

    function lookIn(code) {
        language = code
        choosing = false
        task = Comics.resolve(item, code)
    }

    function show(match) {
        close()
        // The catalogue entry travels with the series (and is stored with it),
        // so comments, reviews and tracking know which title this is.
        const it = dialog.item
        const ids = it.ids || {}
        const extra = { catalog: it.catalog || "comick", catalogId: it.catalogId || it.hid || "", hid: it.hid || "",
                        catalogTitle: it.title || "", titles: (it.titles || []).slice(0, 20), year: it.year || 0 }
        if (ids.anilist) extra.anilist = ids.anilist
        if (ids.mal) extra.mal = ids.mal
        Router.openSeries({ media: dialog.novel ? "novel" : "comic", source: match.source, sourceUrl: match.url, title: match.title,
                            cover: match.cover || it.cover, description: it.description || "",
                            status: it.status || "", type: it.type || "", rating: it.rating || "",
                            hid: it.hid || "", extra: extra }, null)
    }

    Connections {
        target: dialog.task
        function onFinished(result) {
            const preferred = result.matches.filter(m => m.preferred)
            if (preferred.length === 1) dialog.show(preferred[0])
            else if (result.matches.length === 1) dialog.show(result.matches[0])
        }
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, (parent ? parent.width : 460) - 32)
    modal: true
    focus: true
    padding: 24
    title: ""

    Overlay.modal: Rectangle { color: Theme.overlay }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Motion.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.spring }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 0; duration: Motion.fast }
            NumberAnimation { property: "scale"; to: 0.97; duration: Motion.fast }
        }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
        border.width: 1
        border.color: Theme.borderStrong
    }

    contentItem: Column {
        spacing: 16

        Row {
            width: parent.width
            spacing: 14
            Cover {
                width: 54
                height: 78
                radius: 8
                source: dialog.item.cover || ""
            }
            Column {
                width: parent.width - 68
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    width: parent.width
                    text: dialog.item.title || ""
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.textLg
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: !dialog.task || dialog.task.loading ? (dialog.extensions ? qsTr("Looking for it in your extensions…") : qsTr("Looking for it on your sources…"))
                        : !dialog.task.ok ? dialog.task.error
                        : dialog.matches.length === 0 && dialog.extensions ? qsTr("None of your extensions has it.")
                        : dialog.matches.length === 0 && dialog.language ? qsTr("No source in that language has it either.")
                        : dialog.matches.length === 0 ? qsTr("None of your sources has it yet.")
                        : qsTr("Read it from")
                    color: dialog.task && !dialog.task.loading && !dialog.task.ok ? Theme.danger : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                    wrapMode: Text.Wrap
                }
            }
        }

        Spinner {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !!dialog.task && dialog.task.loading
        }

        Column {
            width: parent.width
            spacing: 6
            Repeater {
                model: dialog.matches
                AbstractButton {
                    id: option
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: 54
                    hoverEnabled: true
                    onClicked: dialog.show(modelData)
                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: option.hovered ? Theme.surface3 : Theme.surface2
                        border.width: 1
                        border.color: option.index === 0 ? Theme.alpha(Theme.accent, 0.4) : Theme.border
                    }
                    contentItem: Item {
                        Column {
                            x: 14
                            width: parent.width - 52
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                // The language, when it is not the user's.
                                text: option.modelData.sourceLabel + (option.modelData.language
                                    && option.modelData.language.split("-")[0] !== (dialog.task.result.contentLanguage || "en")
                                    ? " · " + option.modelData.language.toUpperCase() : "")
                                color: Theme.text
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textMd
                                font.weight: Font.DemiBold
                            }
                            Text {
                                width: parent.width
                                text: option.modelData.title
                                color: Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textSm
                                elide: Text.ElideRight
                            }
                        }
                        Icon {
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            name: "chevron-right"
                            size: 16
                            color: Theme.textFaint
                        }
                    }
                }
            }
        }

        // Other languages to look in.
        Column {
            visible: dialog.showLanguages
            width: parent.width
            spacing: 10
            Text {
                width: parent.width
                text: qsTr("Look for it in another language:")
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                wrapMode: Text.Wrap
            }
            Flow {
                width: parent.width
                spacing: 8
                Repeater {
                    model: dialog.showLanguages ? dialog.otherLanguages : []
                    Chip {
                        required property var modelData
                        text: modelData.label
                        onClicked: dialog.lookIn(modelData.value)
                    }
                }
            }
        }

        Flow {
            width: parent.width
            layoutDirection: Qt.RightToLeft
            spacing: 8
            ScButton {
                text: qsTr("Close")
                onClicked: dialog.close()
            }
            ScButton {
                visible: !dialog.extensions && dialog.hasExtensions && !!dialog.task && !dialog.task.loading
                text: qsTr("My extensions")
                iconName: "puzzle"
                variant: "ghost"
                onClicked: dialog.lookInExtensions()
            }
            ScButton {
                visible: !dialog.novel && !!dialog.task && dialog.task.ok && dialog.matches.length > 0 && !dialog.choosing
                         && dialog.otherLanguages.length > 0
                text: qsTr("Other languages")
                iconName: "languages"
                variant: "ghost"
                onClicked: dialog.choosing = true
            }
            ScButton {
                visible: !!dialog.item.catalogUrl
                text: dialog.item.catalog === "anilist" ? "AniList" : dialog.item.catalog === "mal" ? "MyAnimeList" : qsTr("Comick")
                iconName: "external-link"
                variant: "ghost"
                onClicked: App.openUrl(dialog.item.catalogUrl)
            }
        }
    }
}
