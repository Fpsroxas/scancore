import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import ScanCore

SettingsPane {
    id: pane
    title: "Theme"
    subtitle: "Colours, background and transparency"
    iconName: "palette"

    readonly property bool custom: Theme.custom
    // Four across when there is room, otherwise two.
    readonly property int tileWidth: Theme.compact || width < 600 ? Math.floor((width - 36) / 2)
                                                                 : Math.min(150, Math.floor((width - 72) / 5))

    // Changes some of the custom theme's fields.
    function update(changes) {
        App.setSetting("customTheme", Object.assign({}, Theme.customTheme, changes))
    }

    // Starting points for the custom theme: a window colour each.
    readonly property var presets: [
        { name: "Midnight", base: "#15171d" }, { name: "Ocean", base: "#0e1a29" }, { name: "Forest", base: "#12201a" },
        { name: "Plum", base: "#1e1424" }, { name: "Ember", base: "#22150f" }, { name: "Slate", base: "#262a31" },
        { name: "Paper", base: "#f3efe6" }, { name: "Mist", base: "#e9eef4" }, { name: "Blush", base: "#f6e9ec" }
    ]

    // A tiny window drawn in a palette's own colours.
    component ThemePreview: Item {
        property var colors: Theme.palettes.dark
        anchors.fill: parent
        Rectangle {
            anchors.fill: parent
            color: colors.bg
        }
        Rectangle {
            width: parent.width * 0.28
            height: parent.height
            color: colors.sidebar
            Column {
                x: 8
                y: 10
                spacing: 6
                Repeater {
                    model: 4
                    Rectangle { width: 22 - index * 3; height: 4; radius: 2; color: index === 0 ? colors.text : colors.textFaint }
                }
            }
        }
        Column {
            x: parent.width * 0.28 + 10
            y: 10
            spacing: 6
            Rectangle { width: 54; height: 6; radius: 3; color: colors.text }
            Row {
                spacing: 5
                Repeater {
                    model: 3
                    Rectangle {
                        width: 24
                        height: 34
                        radius: 4
                        color: colors.surface3
                        border.width: 1
                        border.color: colors.border
                    }
                }
            }
            Rectangle { width: 40; height: 4; radius: 2; color: colors.textFaint }
        }
    }

    // A labelled slider with its value.
    component Level: Row {
        id: level
        property real value
        property real from: 0
        property real to: 1
        property string suffix: "%"
        signal moved(real value)
        spacing: 12
        ScSlider {
            width: Theme.compact ? pane.width - 110 : 220
            from: level.from
            to: level.to
            value: level.value
            onMoved: level.moved(value)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            text: Math.round(level.value * 100) + level.suffix
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.textSm
            font.features: { "tnum": 1 }
        }
    }

    SettingsGroup {
        title: "Theme"
        Flow {
            x: 12
            width: parent.width - 24
            topPadding: 12
            bottomPadding: 12
            spacing: 12
            Repeater {
                model: [{ value: "system", label: "System", detail: "Follows your device" },
                        { value: "light", label: "Light" }, { value: "dark", label: "Dark" },
                        { value: "black", label: "Black", detail: "For OLED screens" },
                        { value: "custom", label: "Custom", detail: "Your colours" }]
                ChoiceTile {
                    required property var modelData
                    width: pane.tileWidth
                    label: modelData.label
                    detail: modelData.detail || ""
                    selected: App.settings.theme === modelData.value
                    onClicked: App.setSetting("theme", modelData.value)
                    ThemePreview {
                        visible: modelData.value !== "system"
                        colors: modelData.value === "custom" ? Theme.generate(Theme.customBase)
                              : Theme.palettes[modelData.value] || Theme.palettes.dark
                    }
                    // Custom: its background picture, when it has one.
                    Image {
                        anchors.fill: parent
                        visible: modelData.value === "custom" && Theme.backgroundSource.length > 0 && status === Image.Ready
                        source: modelData.value === "custom" ? Theme.backgroundSource : ""
                        sourceSize.width: 300
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        opacity: 0.85
                    }
                    // System: light and dark, split down the middle.
                    Item {
                        anchors.fill: parent
                        visible: modelData.value === "system"
                        ThemePreview { colors: Theme.palettes.light }
                        Item {
                            x: parent.width / 2
                            width: parent.width / 2
                            height: parent.height
                            clip: true
                            ThemePreview {
                                anchors.fill: undefined
                                x: -parent.x
                                width: parent.parent.width
                                height: parent.height
                                colors: Theme.palettes.dark
                            }
                        }
                    }
                }
            }
        }
    }

    SettingsGroup {
        title: "Accent"
        note: "Mono matches the scancore.onl website. Custom takes any colour."
        Flow {
            x: 16
            width: parent.width - 32
            topPadding: 16
            bottomPadding: 14
            spacing: Theme.compact ? 14 : 18
            Repeater {
                model: Theme.accentNames.map(name => ({ value: name, label: name.charAt(0).toUpperCase() + name.slice(1) }))
                    .concat([{ value: "custom", label: "Custom" }])
                AbstractButton {
                    id: swatch
                    required property var modelData
                    readonly property bool isCustom: modelData.value === "custom"
                    readonly property bool selected: (App.settings.accent || "mono") === modelData.value
                    readonly property color fill: isCustom ? Theme.hexOr(App.settings.customAccent, "#7c9cff")
                        : Theme.dark ? Theme.accents[modelData.value].dark : Theme.accents[modelData.value].light
                    width: 52
                    height: 62
                    hoverEnabled: true
                    onClicked: {
                        if (isCustom) accentPicker.edit(fill)
                        App.setSetting("accent", modelData.value)
                    }
                    contentItem: Column {
                        spacing: 7
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 36
                            height: 36
                            radius: 18
                            color: "transparent"
                            border.width: 2
                            border.color: swatch.selected ? Theme.text : swatch.hovered ? Theme.borderStrong : "transparent"
                            Behavior on border.color { ColorAnimation { duration: Motion.fast } }
                            // Custom: a colour wheel around its colour.
                            Image {
                                visible: swatch.isCustom
                                anchors.centerIn: parent
                                width: 28
                                height: 28
                                source: "image://colorwheel/" + Math.round(56 * Screen.devicePixelRatio)
                                sourceSize: Qt.size(Math.round(56 * Screen.devicePixelRatio), Math.round(56 * Screen.devicePixelRatio))
                                scale: swatch.pressed ? 0.88 : swatch.hovered ? 1.08 : 1
                                Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutBack } }
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: swatch.isCustom ? 16 : 28
                                height: width
                                radius: width / 2
                                color: swatch.fill
                                border.width: swatch.isCustom ? 2 : 0
                                border.color: "white"
                                scale: swatch.isCustom ? 1 : swatch.pressed ? 0.88 : swatch.hovered ? 1.08 : 1
                                Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutBack } }
                                Icon {
                                    anchors.centerIn: parent
                                    visible: swatch.selected && !swatch.isCustom
                                    name: "check"
                                    size: 14
                                    color: swatch.isCustom ? "transparent"
                                         : Theme.dark ? Theme.accents[swatch.modelData.value].on.dark : Theme.accents[swatch.modelData.value].on.light
                                }
                            }
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: swatch.modelData.label
                            color: swatch.selected ? Theme.text : Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.textXs + 1
                            font.weight: swatch.selected ? Font.DemiBold : Font.Normal
                        }
                    }
                    background: null
                }
            }
        }
    }

    // ------------------------------------------------------------ custom

    SettingsGroup {
        visible: pane.custom
        title: "Custom theme"
        note: "Every colour in the app is made from the window colour; its brightness decides whether the theme is light or dark."

        SettingRow {
            iconName: "palette"
            title: "Window colour"
            description: String(Theme.customBase).toUpperCase() + (Theme.dark ? " · dark" : " · light")
            Row {
                spacing: 10
                AbstractButton {
                    width: 36
                    height: 36
                    hoverEnabled: true
                    onClicked: basePicker.edit(Theme.customBase)
                    background: Rectangle {
                        radius: Theme.radiusMd
                        color: Theme.customBase
                        border.width: 1
                        border.color: parent.hovered ? Theme.text : Theme.borderStrong
                    }
                }
                ScButton { text: "Change"; onClicked: basePicker.edit(Theme.customBase) }
            }
        }
        // Quick starting points.
        Flow {
            x: 16
            width: parent.width - 32
            bottomPadding: 16
            spacing: 8
            Repeater {
                model: pane.presets
                Chip {
                    required property var modelData
                    text: modelData.name
                    dot: modelData.base
                    selected: String(Theme.customBase) === modelData.base
                    onClicked: pane.update({ base: modelData.base })
                }
            }
        }
        SettingRow {
            iconName: "layers"
            title: "Panels"
            description: "How solid the sidebar and cards are over a background picture or a see-through window."
            Level {
                value: Theme.panelOpacity
                from: 0.2
                onMoved: (v) => pane.update({ panelOpacity: Math.round(v * 100) / 100 })
            }
        }
        SettingRow {
            visible: App.platform !== "android"
            iconName: "monitor"
            title: "Window transparency"
            description: Customize.translucentWindow ? "See your desktop through the window."
                : (Number(Theme.customTheme.windowOpacity) < 1
                   ? "Takes effect when SCANCORE restarts." : "See your desktop through the window. Takes effect after a restart.")
            Row {
                spacing: 8
                Level {
                    value: Theme.clamp(Number(Theme.customTheme.windowOpacity), 0.3, 1, 1)
                    from: 0.3
                    suffix: "% solid"
                    onMoved: (v) => pane.update({ windowOpacity: Math.round(v * 100) / 100 })
                }
                ScButton {
                    visible: Customize.canRestart && !Customize.translucentWindow && Number(Theme.customTheme.windowOpacity) < 1
                    compact: true
                    variant: "primary"
                    text: "Restart"
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: Customize.restart()
                }
            }
        }
    }

    SettingsGroup {
        visible: pane.custom
        title: "Background"
        note: "PNG, JPEG, GIF or WebP, up to 40 MB. SCANCORE keeps its own copy. Animated backgrounds pause while a reader or the player is open."

        Item {
            width: parent.width
            height: Theme.backgroundSource ? 168 : 0
            visible: Theme.backgroundSource.length > 0
            Rectangle {
                x: 16
                y: 16
                width: parent.width - 32
                height: parent.height - 16
                radius: Theme.radiusMd
                color: Theme.base
                clip: true
                Image {
                    anchors.fill: parent
                    source: Theme.backgroundSource
                    sourceSize.width: 720
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }
                Rectangle {
                    anchors.fill: parent
                    color: Theme.alpha(Theme.base, Theme.clamp(Number(Theme.customTheme.backgroundDim), 0, 0.95, 0.55))
                }
                Text {
                    anchors.centerIn: parent
                    text: "Aa"
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 34
                    font.weight: Font.Bold
                }
            }
        }
        SettingRow {
            iconName: "sparkles"
            title: Theme.backgroundSource ? "Background picture" : "No background picture"
            description: Theme.backgroundSource ? (Theme.customTheme.backgroundAnimated ? "Animated" : "Picture") : "Use a picture or GIF behind everything."
            Row {
                spacing: 8
                ScButton { text: Theme.backgroundSource ? "Change" : "Choose…"; iconName: "folder-open"; onClicked: backgroundDialog.open() }
                ScButton {
                    visible: Theme.backgroundSource.length > 0
                    text: "Remove"
                    variant: "ghost"
                    onClicked: pane.update({ background: "", backgroundAnimated: false })
                }
            }
        }
        SettingRow {
            visible: Theme.backgroundSource.length > 0
            title: "Fit"
            Segmented {
                options: [{ value: "cover", label: "Fill" }, { value: "contain", label: "Fit" },
                          { value: "tile", label: "Tile" }, { value: "stretch", label: "Stretch" }]
                value: Theme.customTheme.backgroundFit || "cover"
                onPicked: (v) => pane.update({ backgroundFit: v })
            }
        }
        SettingRow {
            visible: Theme.backgroundSource.length > 0
            title: "Dim"
            description: "Darkens (or lightens) the picture towards the window colour, for readable text."
            Level {
                value: Theme.clamp(Number(Theme.customTheme.backgroundDim), 0, 0.95, 0.55)
                to: 0.95
                onMoved: (v) => pane.update({ backgroundDim: Math.round(v * 100) / 100 })
            }
        }
        SettingRow {
            visible: Theme.backgroundSource.length > 0
            title: "Blur"
            Level {
                value: Theme.clamp(Number(Theme.customTheme.backgroundBlur), 0, 1, 0)
                onMoved: (v) => pane.update({ backgroundBlur: Math.round(v * 100) / 100 })
            }
        }
    }

    SettingsGroup {
        title: "Share"
        note: "A .sct file holds the colours, transparency and background picture in one file that works on every system."
            + (App.platform !== "android" ? " You can also drop one on the window." : "")
        SettingRow {
            visible: pane.custom
            iconName: "type"
            title: "Name"
            TextField {
                width: Theme.compact ? pane.width - 70 : 220
                height: 36
                text: Theme.customTheme.name || ""
                placeholderText: "My theme"
                placeholderTextColor: Theme.textFaint
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.textSm
                leftPadding: 12
                selectByMouse: true
                maximumLength: 80
                background: Rectangle {
                    radius: Theme.radiusMd
                    color: Theme.surface2
                    border.width: 1
                    border.color: parent.activeFocus ? Theme.accent : Theme.border
                }
                onEditingFinished: if (text !== (Theme.customTheme.name || "")) pane.update({ name: text })
            }
        }
        SettingRow {
            iconName: "shield-check"
            title: "Theme file"
            description: pane.custom ? "Save this theme, or use one someone shared." : "Use a theme someone shared."
            Row {
                spacing: 8
                ScButton {
                    visible: pane.custom
                    text: "Export…"
                    iconName: "download"
                    onClicked: exportDialog.open()
                }
                ScButton { text: "Import…"; variant: pane.custom ? "ghost" : "secondary"; onClicked: importDialog.open() }
            }
        }
    }

    // ------------------------------------------------------------ dialogs

    ColorPicker {
        id: basePicker
        title: "Window colour"
        onChanged: (c) => pane.update({ base: String(c).slice(0, 7) })
    }
    ColorPicker {
        id: accentPicker
        title: "Accent colour"
        swatches: ["#f2f2f3", "#7c9cff", "#818cf8", "#a78bfa", "#c084fc", "#f472b6", "#fb7185", "#f87171",
                   "#fb923c", "#fbbf24", "#a3e635", "#34d399", "#2dd4bf", "#38bdf8"]
        onChanged: (c) => {
            App.setSetting("customAccent", String(c).slice(0, 7))
            App.setSetting("accent", "custom")
        }
    }

    FileDialog {
        id: backgroundDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["Pictures and GIFs (*.png *.jpg *.jpeg *.gif *.webp)", "All files (*)"]
        onAccepted: {
            const result = Customize.useBackground(selectedFile)
            if (!result.ok) {
                Router.toast(result.error, "error")
                return
            }
            const changes = { background: result.url, backgroundAnimated: result.animated }
            // A first picture starts dimmed enough to read text over.
            if (Theme.customTheme.backgroundDim === undefined) changes.backgroundDim = 0.55
            pane.update(changes)
        }
    }
    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "sct"
        nameFilters: ["SCANCORE theme (*.sct)"]
        selectedFile: (Theme.customTheme.name || "scancore-theme").replace(/[^\w\- ]+/g, "").trim().replace(/\s+/g, "-").toLowerCase() + ".sct"
        onAccepted: {
            const theme = Object.assign({}, Theme.customTheme, { accent: Theme.customAccent ? String(Theme.accent).slice(0, 7) : "" })
            pane.watch(Customize.exportTheme(theme, selectedFile), (name) => "Saved " + name)
        }
    }
    FileDialog {
        id: importDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["SCANCORE theme (*.sct)", "All files (*)"]
        onAccepted: pane.watch(Customize.importTheme(selectedFile), (theme) => { Router.applyTheme(theme); return "" })
    }

    // Held here so the garbage collector does not cancel it.
    property var task: null
    property var describe: null
    function watch(t, f) {
        describe = f
        task = t
    }
    Connections {
        target: pane.task
        function onFinished(result) {
            const message = pane.describe ? pane.describe(result) : ""
            if (message) Router.toast(message, "success")
        }
        function onFailed(error) { Router.toast(error, "error") }
    }
}
