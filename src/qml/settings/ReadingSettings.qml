import QtQuick
import QtQuick.Controls.Basic
import ScanCore

SettingsPane {
    id: pane
    title: qsTr("Reading")
    subtitle: qsTr("Comic and novel readers")
    iconName: "book-open"

    readonly property int tileWidth: Theme.compact ? Math.floor((width - 36) / 2) : 168
    readonly property var look: Theme.readingThemes[App.settings.novelTheme] || Theme.readingThemes.app
    readonly property int sliderWidth: Theme.compact ? width - 80 : 220

    // A value next to its slider.
    component Readout: Text {
        color: Theme.textMuted
        font.family: Theme.fontSans
        font.pixelSize: Theme.textSm
        font.features: { "tnum": 1 }
        anchors.verticalCenter: parent.verticalCenter
    }

    SettingsGroup {
        title: qsTr("Comic reader")
        note: qsTr("While reading: M switches layout, Ctrl + scroll changes the strip width (or zooms a page), and F goes full screen. Each series can keep its own settings from the reader's settings button.")

        Flow {
            x: 12
            width: parent.width - 24
            topPadding: 12
            bottomPadding: 12
            spacing: 12
            ChoiceTile {
                width: pane.tileWidth
                label: qsTr("Vertical strip")
                detail: qsTr("Webtoons, manhwa")
                selected: App.settings.readerMode !== "paged"
                onClicked: App.setSetting("readerMode", "strip")
                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    Repeater {
                        model: 3
                        Rectangle { width: 34; height: 22; radius: 3; color: index === 1 ? Theme.borderStrong : Theme.surface3 }
                    }
                }
            }
            ChoiceTile {
                width: pane.tileWidth
                label: qsTr("Pages")
                detail: qsTr("Manga")
                selected: App.settings.readerMode === "paged"
                onClicked: App.setSetting("readerMode", "paged")
                Row {
                    anchors.centerIn: parent
                    spacing: 4
                    Repeater {
                        model: 2
                        Rectangle { width: 30; height: 44; radius: 3; color: index === 0 ? Theme.borderStrong : Theme.surface3 }
                    }
                }
            }
        }
        SettingRow {
            title: qsTr("Page direction")
            description: qsTr("For the Pages layout.")
            Segmented {
                options: [{ value: "ltr", label: qsTr("Left to right") }, { value: "rtl", label: qsTr("Right to left") }]
                value: App.settings.readerDirection || "ltr"
                onPicked: (v) => App.setSetting("readerDirection", v)
            }
        }
        SettingRow {
            visible: App.platform !== "android"
            title: qsTr("Two pages side by side")
            description: qsTr("In the Pages layout, when the window is wider than it is tall. Covers and double-page spreads stay alone.")
            Toggle { checked: App.settings.readerSpread === "double"; onToggled: App.setSetting("readerSpread", checked ? "double" : "single") }
        }
        SettingRow {
            title: qsTr("Trim page borders")
            description: qsTr("Cuts the plain white or black margin around scanned pages in the Pages layout.")
            Toggle { checked: App.settings.readerCrop === true; onToggled: App.setSetting("readerCrop", checked) }
        }
        SettingRow {
            title: qsTr("Strip width")
            description: qsTr("How wide the vertical strip is on large screens.")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 360; to: 2000; stepSize: 20; value: App.settings.readerWidth; onMoved: App.setSetting("readerWidth", value) }
                Readout { text: App.settings.readerWidth + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Gap between panels")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 0; to: 40; stepSize: 2; value: App.settings.readerGap || 0; onMoved: App.setSetting("readerGap", value) }
                Readout { text: (App.settings.readerGap || 0) + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Progress rail")
            description: qsTr("A thin line on the right edge showing how far through the chapter you are.")
            Toggle { checked: App.settings.readerProgressBar !== false; onToggled: App.setSetting("readerProgressBar", checked) }
        }
    }

    SettingsGroup {
        title: qsTr("Novel reader")

        // Live preview in the chosen type and reading theme.
        Item {
            width: parent.width
            height: preview.height + 24
            Rectangle {
                id: preview
                x: 12
                y: 12
                width: parent.width - 24
                height: sample.implicitHeight + 40
                radius: Theme.radiusMd
                color: pane.look.bg
                border.width: 1
                border.color: Theme.border
                Behavior on color { ColorAnimation { duration: Motion.normal } }
                Text {
                    id: sample
                    x: 20
                    y: 20
                    width: parent.width - 40
                    text: qsTr("The lamps of the old city came on one by one, and for a moment the whole street seemed to hold its breath. ")
                        + qsTr("She turned the page and kept reading.")
                    color: pane.look.text
                    font.family: App.settings.novelFont || Theme.fontSerif
                    font.pixelSize: App.settings.novelSize || 19
                    lineHeight: App.settings.novelLineHeight || 1.5
                    horizontalAlignment: App.settings.novelJustify === true ? Text.AlignJustify : Text.AlignLeft
                    wrapMode: Text.Wrap
                }
            }
        }
        SettingRow {
            title: qsTr("Reading theme")
            Row {
                spacing: 8
                Repeater {
                    model: ["app", "paper", "sepia", "dark", "black"]
                    AbstractButton {
                        id: chip
                        required property string modelData
                        readonly property var colors: Theme.readingThemes[modelData]
                        readonly property bool selected: (App.settings.novelTheme || "app") === modelData
                        width: Theme.compact ? 44 : 50
                        height: 50
                        hoverEnabled: true
                        onClicked: App.setSetting("novelTheme", modelData)
                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: chip.colors.bg
                            border.width: chip.selected ? 2 : 1
                            border.color: chip.selected ? Theme.accent : chip.hovered ? Theme.borderStrong : Theme.border
                        }
                        contentItem: Column {
                            topPadding: 6
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Aa"
                                color: chip.colors.text
                                font.family: Theme.fontSerif
                                font.pixelSize: 15
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: chip.colors.label
                                color: chip.colors.muted
                                font.family: Theme.fontSans
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }
        }
        SettingRow {
            title: qsTr("Font")
            Segmented {
                options: [{ value: "Literata", label: qsTr("Serif") }, { value: "DM Sans", label: qsTr("Sans") }, { value: "Space Grotesk", label: qsTr("Grotesk") }]
                value: App.settings.novelFont
                onPicked: (v) => App.setSetting("novelFont", v)
            }
        }
        SettingRow {
            title: qsTr("Text size")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 13; to: 34; stepSize: 1; value: App.settings.novelSize; onMoved: App.setSetting("novelSize", value) }
                Readout { text: App.settings.novelSize + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Line spacing")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 1.2; to: 2.2; stepSize: 0.05; value: App.settings.novelLineHeight || 1.5; onMoved: App.setSetting("novelLineHeight", Math.round(value * 100) / 100) }
                Readout { text: Number(App.settings.novelLineHeight || 1.5).toFixed(2) }
            }
        }
        SettingRow {
            visible: !Theme.compact
            title: qsTr("Column width")
            Row {
                spacing: 12
                ScSlider { width: pane.sliderWidth; from: 480; to: 1100; stepSize: 20; value: App.settings.novelWidth || 720; onMoved: App.setSetting("novelWidth", value) }
                Readout { text: (App.settings.novelWidth || 720) + qsTr(" px") }
            }
        }
        SettingRow {
            title: qsTr("Justify text")
            description: qsTr("Even right edges, like a printed book.")
            Toggle { checked: App.settings.novelJustify === true; onToggled: App.setSetting("novelJustify", checked) }
        }
    }
}
