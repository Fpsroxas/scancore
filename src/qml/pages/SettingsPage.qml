import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// Settings. Wide windows list the categories beside one long page that
// scrolls through all of them; phones show the categories as a list and
// open each on its own page (SettingsDetail.qml).
Item {
    id: page

    readonly property var sections: [
        { key: "account", file: "AccountSettings.qml", title: "Account", subtitle: "Optional sync with scancore.onl", icon: "user-round", tint: "sky" },
        { key: "appearance", file: "AppearanceSettings.qml", title: "Appearance", subtitle: "Theme, accent colour and motion", icon: "palette", tint: "violet" },
        { key: "reading", file: "ReadingSettings.qml", title: "Reading", subtitle: "Comic and novel readers", icon: "book-open", tint: "ember" },
        { key: "anime", file: "AnimeSettings.qml", title: "Anime", subtitle: "Playback and skipping", icon: "tv", tint: "rose" },
        { key: "library", file: "LibrarySettings.qml", title: "Library", subtitle: "New chapters and notifications", icon: "library", tint: "green" },
        { key: "sources", file: "SourcesSettings.qml", title: "Sources", subtitle: "The sites SCANCORE reads from", icon: "globe", tint: "amber" },
        { key: "storage", file: "StorageSettings.qml", title: "Storage", subtitle: "Cache, downloads and backups", icon: "hard-drive", tint: "mono" },
        { key: "about", file: "AboutSettings.qml", title: "About", subtitle: "Version, updates and help", icon: "info", tint: "mono" }
    ]

    function tint(name) {
        if (name === "green") return Theme.success
        if (name === "amber") return Theme.warning
        if (name === "mono" || !Theme.accents[name]) return Theme.textMuted
        return Theme.dark ? Theme.accents[name].dark : Theme.accents[name].light
    }

    // One line under each category on the phone list.
    function summary(key) {
        App.settings
        switch (key) {
        case "appearance": {
            const theme = { system: "System", light: "Light", dark: "Dark", black: "Black" }[App.settings.theme] || "Dark"
            const accent = String(App.settings.accent || "mono")
            return theme + " theme · " + accent.charAt(0).toUpperCase() + accent.slice(1) + " accent"
        }
        case "reading":
            return (App.settings.readerMode === "paged" ? "Pages" : "Vertical strip") + " · "
                + ({ "Literata": "Serif", "DM Sans": "Sans", "Space Grotesk": "Grotesk" }[App.settings.novelFont] || "Serif") + " text"
        case "anime":
            return "Auto-next " + (App.settings.autoNext !== false ? "on" : "off") + " · Auto-skip " + (App.settings.autoSkip === true ? "on" : "off")
        case "library": {
            const hours = App.settings.libraryCheckHours
            return hours > 0 ? "Checks for new chapters every " + (hours === 1 ? "hour" : hours === 24 ? "day" : hours + " hours") : "Update checks off"
        }
        case "sources": return "Source list version " + App.manifestVersion
        case "storage": return "Cache, downloads and backup"
        case "about": return "Version " + App.version
        }
        return ""
    }

    function open(section) {
        Router.shell.push("pages/SettingsDetail.qml", { section: section, tint: page.tint(section.tint) })
    }

    Loader {
        anchors.fill: parent
        sourceComponent: Theme.compact ? phoneLayout : wideLayout
    }

    // ------------------------------------------------------------ wide

    Component {
        id: wideLayout
        Item {
            id: wide
            readonly property bool showNav: width >= 860
            property int current: 0

            function scrollTo(i) {
                const target = panes.itemAt(i)
                if (target) flick.scrollTo(Math.min(flick.maxY, target.y + content.y - 24), true)
            }

            Column {
                id: nav
                visible: wide.showNav
                x: Theme.gutter
                y: 24
                width: 212
                spacing: 2
                Text {
                    text: "Settings"
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.text3xl
                    font.weight: Font.Bold
                    font.letterSpacing: -0.6
                }
                Text {
                    bottomPadding: 20
                    text: "Stored on this computer"
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                }
                Repeater {
                    model: page.sections
                    AbstractButton {
                        id: item
                        required property var modelData
                        required property int index
                        readonly property bool active: wide.current === index
                        width: nav.width
                        height: 40
                        hoverEnabled: true
                        onClicked: wide.scrollTo(index)
                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: item.active ? Theme.surface2 : item.hovered ? Theme.alpha(Theme.text, 0.04) : "transparent"
                            border.width: item.active ? 1 : 0
                            border.color: Theme.border
                        }
                        contentItem: Row {
                            leftPadding: 8
                            spacing: 11
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 26
                                height: 26
                                radius: 8
                                color: Theme.alpha(page.tint(item.modelData.tint), item.active ? 0.2 : 0.12)
                                Icon { anchors.centerIn: parent; name: item.modelData.icon; size: 14; color: page.tint(item.modelData.tint) }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: item.modelData.title
                                color: item.active ? Theme.text : Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textMd
                                font.weight: item.active ? Font.DemiBold : Font.Medium
                            }
                        }
                    }
                }
            }

            SmoothFlickable {
                id: flick
                anchors.fill: parent
                anchors.leftMargin: wide.showNav ? nav.x + nav.width + 24 : 0
                contentHeight: content.y + content.height + 120
                onContentYChanged: {
                    // The last category whose heading has scrolled near the top.
                    let found = 0
                    for (let i = 0; i < panes.count; ++i) {
                        const p = panes.itemAt(i)
                        if (p && p.y + content.y - 140 <= contentY) found = i
                    }
                    if (contentY >= maxY - 4) found = panes.count - 1
                    wide.current = found
                }

                Column {
                    id: content
                    x: wide.showNav ? 8 : Theme.gutter
                    y: wide.showNav ? 30 : 0
                    width: Math.min(flick.width - x - Theme.gutter, 780)
                    spacing: 48

                    PageHeader {
                        visible: !wide.showNav
                        x: -Theme.gutter
                        width: content.width + 2 * Theme.gutter
                        title: "Settings"
                        subtitle: "Stored on this computer"
                    }

                    Repeater {
                        id: panes
                        model: page.sections
                        Loader {
                            required property var modelData
                            width: content.width
                            Component.onCompleted: setSource(Qt.resolvedUrl("../settings/" + modelData.file), { tint: page.tint(modelData.tint) })
                        }
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------ phone

    Component {
        id: phoneLayout
        SmoothFlickable {
            contentHeight: list.height + 40

            Column {
                id: list
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 22

                PageHeader {
                    x: -Theme.gutter
                    width: list.width + 2 * Theme.gutter
                    title: "Settings"
                    subtitle: "Stored on this device"
                }

                // Account summary; opens the account page.
                AbstractButton {
                    id: accountCard
                    width: parent.width
                    height: 76
                    onClicked: page.open(page.sections[0])
                    background: Rectangle {
                        radius: Theme.radiusLg
                        color: accountCard.pressed ? Theme.surface2 : Theme.surface
                        border.width: 1
                        border.color: Theme.border
                    }
                    contentItem: Item {
                        Rectangle {
                            x: 14
                            anchors.verticalCenter: parent.verticalCenter
                            width: 46
                            height: 46
                            radius: 23
                            color: Account.signedIn ? Theme.accent : Theme.alpha(page.tint("sky"), 0.14)
                            Text {
                                anchors.centerIn: parent
                                visible: Account.signedIn
                                text: String(Account.user.username || Account.user.email || "?").slice(0, 1).toUpperCase()
                                color: Theme.accentInk
                                font.family: Theme.fontDisplay
                                font.pixelSize: 20
                                font.weight: Font.Bold
                            }
                            Icon { anchors.centerIn: parent; visible: !Account.signedIn; name: "user-round"; size: 20; color: page.tint("sky") }
                        }
                        Column {
                            x: 74
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - x - 40
                            spacing: 3
                            Text {
                                width: parent.width
                                text: Account.signedIn ? (Account.user.username || Account.user.email) : "Sign in to sync"
                                color: Theme.text
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.textLg
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: Account.signedIn ? (Account.syncing ? "Syncing…" : Account.lastSync ? "Synced " + App.relativeTime(Number(Account.lastSync)) : "scancore.onl account")
                                                       : "Library and anime progress on every device"
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
                            size: 18
                            color: Theme.textFaint
                        }
                    }
                }

                Repeater {
                    model: [[1, 2, 3], [4, 5, 6], [7]]
                    SettingsGroup {
                        required property var modelData
                        Repeater {
                            model: modelData
                            SettingRow {
                                required property int modelData
                                readonly property var section: page.sections[modelData]
                                iconName: section.icon
                                iconTint: page.tint(section.tint)
                                title: section.title
                                description: page.summary(section.key)
                                clickable: true
                                onClicked: page.open(section)
                            }
                        }
                    }
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "SCANCORE " + App.version + " · Medium Dev"
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textXs + 1
                }
            }
        }
    }
}
