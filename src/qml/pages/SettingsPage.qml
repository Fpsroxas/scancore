import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import ScanCore

Item {
    id: page
    property var cacheTask: App.cacheSize()
    property var updateTask: null
    property var sourcesTask: null
    property var backupTask: null

    component SectionTitle: Text {
        topPadding: 30
        bottomPadding: 6
        color: Theme.text
        font.family: Theme.fontDisplay
        font.pixelSize: Theme.textXl
        font.weight: Font.DemiBold
    }

    function report(task, success) {
        task.finished.connect(() => Router.toast(typeof success === "function" ? success(task.result) : success, "success"))
        task.failed.connect((error) => Router.toast(error, "error"))
        return task
    }

    SmoothFlickable {
        anchors.fill: parent
        contentHeight: column.height + 80

        Column {
            id: column
            x: Theme.gutter
            width: Math.min(parent.width - 2 * Theme.gutter, 860)

            PageHeader {
                x: -Theme.gutter
                width: column.width + 2 * Theme.gutter
                title: "Settings"
                subtitle: "Everything is stored on this computer"
            }

            SectionTitle { text: "scancore.onl account" }
            SettingRow {
                visible: Account.signedIn
                title: "Signed in as " + (Account.user.username || Account.user.email || "")
                description: Account.syncing ? "Syncing…"
                    : Account.lastSync ? "Library and reading history synced " + App.relativeTime(Number(Account.lastSync)) + "."
                    : "Your website library and reading history sync with this app."
                Row {
                    spacing: 8
                    ScButton {
                        text: "Sync now"
                        iconName: "refresh-cw"
                        loading: Account.syncing
                        onClicked: page.report(Account.sync(), (r) => "Synced: " + r.libraryImported + " saved, " + r.historyImported + " read, " + r.pushed + " sent to the website")
                    }
                    ScButton { text: "Sign out"; variant: "ghost"; onClicked: Account.signOut() }
                }
            }
            Column {
                visible: !Account.signedIn
                width: parent.width
                spacing: 14
                topPadding: 4
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Optional. Bring your scancore.onl library and comic reading history into this app, and send saves made here back to the website."
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.textSm
                }
                AuthForm {
                    width: Math.min(parent.width, 420)
                    onDone: Router.toast("Signed in", "success")
                }
            }

            SectionTitle { text: "Appearance" }
            SettingRow {
                title: "Theme"
                Segmented {
                    options: [{ value: "system", label: "System", icon: "monitor" }, { value: "light", label: "Light", icon: "sun" },
                              { value: "dark", label: "Dark", icon: "moon" }, { value: "black", label: "Black" }]
                    value: App.settings.theme
                    onPicked: (v) => App.setSetting("theme", v)
                }
            }
            SettingRow {
                title: "Accent"
                description: "Mono matches the SCANCORE website."
                Row {
                    spacing: 10
                    Repeater {
                        model: ["mono", "violet", "ember", "sky", "rose"]
                        Rectangle {
                            required property string modelData
                            width: 28
                            height: 28
                            radius: 14
                            color: Theme.dark ? Theme.accents[modelData].dark : Theme.accents[modelData].light
                            border.width: App.settings.accent === modelData ? 3 : 0
                            border.color: Theme.bg
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: -3
                                radius: width / 2
                                color: "transparent"
                                border.width: 2
                                border.color: Theme.text
                                visible: App.settings.accent === modelData
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.setSetting("accent", modelData) }
                        }
                    }
                }
            }
            SettingRow {
                title: "Reduce motion"
                description: "Turns off page transitions, hover lifts and other animations."
                Toggle { checked: App.settings.reducedMotion === true; onToggled: App.setSetting("reducedMotion", checked) }
            }
            SettingRow {
                title: "Show adult sources"
                description: "Lists sources marked as adult in the comic source picker."
                Toggle { checked: App.settings.showNsfwSources === true; onToggled: App.setSetting("showNsfwSources", checked) }
            }

            SectionTitle { text: "Comic reader" }
            SettingRow {
                title: "Layout"
                description: "Vertical strip suits webtoons and manhwa; pages suit manga. Press M while reading to switch."
                Segmented {
                    options: [{ value: "strip", label: "Vertical", icon: "move-vertical" }, { value: "paged", label: "Pages", icon: "layers" }]
                    value: App.settings.readerMode
                    onPicked: (v) => App.setSetting("readerMode", v)
                }
            }
            SettingRow {
                title: "Strip width · " + App.settings.readerWidth + " px"
                description: "Ctrl + scroll while reading adjusts it too."
                ScSlider { width: 240; from: 360; to: 2000; stepSize: 20; value: App.settings.readerWidth; onMoved: App.setSetting("readerWidth", value) }
            }
            SettingRow {
                title: "Page direction"
                Segmented {
                    options: [{ value: "ltr", label: "Left to right" }, { value: "rtl", label: "Right to left" }]
                    value: App.settings.readerDirection
                    onPicked: (v) => App.setSetting("readerDirection", v)
                }
            }

            SectionTitle { text: "Novel reader" }
            SettingRow {
                title: "Font"
                Segmented {
                    options: [{ value: "Literata", label: "Serif" }, { value: "DM Sans", label: "Sans" }, { value: "Space Grotesk", label: "Grotesk" }]
                    value: App.settings.novelFont
                    onPicked: (v) => App.setSetting("novelFont", v)
                }
            }
            SettingRow {
                title: "Text size · " + App.settings.novelSize
                ScSlider { width: 240; from: 13; to: 34; stepSize: 1; value: App.settings.novelSize; onMoved: App.setSetting("novelSize", value) }
            }
            SettingRow {
                title: "Reading theme"
                Select {
                    model: [{ value: "app", label: "Match the app" }, { value: "paper", label: "Paper" }, { value: "sepia", label: "Sepia" },
                            { value: "dark", label: "Dark" }, { value: "black", label: "Black" }]
                    value: App.settings.novelTheme
                    onPicked: (v) => App.setSetting("novelTheme", v)
                }
            }

            SectionTitle { text: "Anime" }
            SettingRow {
                title: "Play the next episode automatically"
                Toggle { checked: App.settings.autoNext !== false; onToggled: App.setSetting("autoNext", checked) }
            }
            SettingRow {
                title: "Skip intros and recaps automatically"
                description: "Uses AniSkip timestamps when they exist. Otherwise a Skip button appears."
                Toggle { checked: App.settings.autoSkip === true; onToggled: App.setSetting("autoSkip", checked) }
            }

            SectionTitle { text: "Library" }
            SettingRow {
                title: "Check for new chapters"
                description: "Saved comics and novels are checked in the background while SCANCORE is open."
                Select {
                    model: [{ value: 0, label: "Never" }, { value: 1, label: "Every hour" }, { value: 2, label: "Every 2 hours" },
                            { value: 6, label: "Every 6 hours" }, { value: 24, label: "Once a day" }]
                    value: App.settings.libraryCheckHours
                    onPicked: (v) => App.setSetting("libraryCheckHours", v)
                }
            }
            SettingRow {
                title: "Desktop notifications for new chapters"
                Toggle { checked: App.settings.notifyUpdates !== false; onToggled: App.setSetting("notifyUpdates", checked) }
            }
            SettingRow {
                title: "Keep running in the tray when the window closes"
                description: "Lets update checks and downloads continue in the background."
                Toggle { checked: App.settings.closeToTray === true; onToggled: App.setSetting("closeToTray", checked) }
            }

            SectionTitle { text: "Sources" }
            SettingRow {
                title: "Source list version " + App.manifestVersion
                description: "Site addresses and availability update from the SCANCORE repository without a new release."
                ScButton {
                    text: "Check now"
                    iconName: "refresh-cw"
                    loading: page.sourcesTask ? page.sourcesTask.loading : false
                    onClicked: page.sourcesTask = page.report(App.refreshSources(), (r) => r.changed ? "Sources updated to version " + r.version : "Sources are up to date")
                }
            }
            SettingRow {
                visible: App.platform !== "android"
                title: "Extended novel sources" + (!Extensions.novelPackAvailable ? " · unavailable in this build" : Extensions.novelPackInstalled ? " · installed" : "")
                description: !Extensions.novelPackAvailable ? "The downloadable Python novel-source pack is disabled in this build."
                    : Extensions.busy ? Extensions.status
                    : Extensions.novelPackInstalled ? Novels.sources().filter(s => s.extended).length + " more novel sites from lightnovel-crawler. Pick them from “More sources” on the Novels page; pasted book links from any of them open directly."
                    : "Adds about 260 more novel sites using lightnovel-crawler. Downloads a private copy of Python and the crawler (about 400 MB) into the app data folder." + (Extensions.status ? " Last attempt: " + Extensions.status : "")
                Row {
                    spacing: 8
                    ScButton {
                        visible: Extensions.novelPackAvailable && !Extensions.novelPackInstalled
                        variant: "primary"
                        text: "Install"
                        iconName: "download"
                        loading: Extensions.busy
                        enabled: !Extensions.busy
                        onClicked: page.report(Extensions.installNovelPack(), (r) => "Extended novel sources installed")
                    }
                    ScButton {
                        visible: Extensions.novelPackAvailable && Extensions.novelPackInstalled
                        text: "Remove"
                        variant: "ghost"
                        onClicked: Extensions.removeNovelPack()
                    }
                }
            }
            Repeater {
                model: { Extensions.novelPackInstalled; return Comics.sources().map(s => Object.assign({ kind: "Comics" }, s)).concat(Novels.sources().filter(s => !s.extended).map(s => Object.assign({ kind: "Novels" }, s))) }
                SettingRow {
                    required property var modelData
                    title: modelData.label
                    description: modelData.kind + " · " + modelData.base.replace("https://", "") + (modelData.enabled ? "" : " · " + modelData.disabledReason)
                    Row {
                        spacing: 6
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 8; height: 8; radius: 4
                            color: modelData.enabled ? Theme.success : Theme.textFaint
                        }
                        IconButton { iconName: "globe"; tip: "Open in SCANCORE to pass a browser check"; onClicked: Router.openSiteCheck(modelData.base + "/") }
                        IconButton { iconName: "external-link"; tip: "Open site"; onClicked: App.openUrl(modelData.base) }
                    }
                }
            }

            SectionTitle { text: "Storage" }
            SettingRow {
                title: "Image cache · " + (page.cacheTask && page.cacheTask.ok ? App.formatBytes(page.cacheTask.result) : "…")
                description: "Covers and pages you have viewed, kept for faster loading. Limited to " + App.settings.imageCacheMB + " MB."
                ScButton {
                    text: "Clear"
                    onClicked: {
                        App.clearCache()
                        Router.toast("Cache cleared")
                        refreshCache.start()
                    }
                }
            }
            Timer { id: refreshCache; interval: 800; onTriggered: page.cacheTask = App.cacheSize() }
            SettingRow {
                title: "Downloads · " + Downloads.sizeOnDisk()
                description: Downloads.folder()
                ScButton { text: "Open folder"; iconName: "folder-open"; onClicked: App.openFolder(Downloads.folder()) }
            }
            SettingRow {
                title: "Backup"
                description: "Your library, history, reading positions and settings in one file."
                Row {
                    spacing: 8
                    ScButton { text: "Export"; iconName: "download"; onClicked: exportDialog.open() }
                    ScButton { text: "Restore"; onClicked: importDialog.open() }
                }
            }
            FileDialog {
                id: exportDialog
                fileMode: FileDialog.SaveFile
                defaultSuffix: "scancore"
                nameFilters: ["SCANCORE backup (*.scancore)"]
                selectedFile: "scancore-backup-" + Qt.formatDate(new Date(), "yyyy-MM-dd") + ".scancore"
                onAccepted: page.backupTask = page.report(App.exportBackup(selectedFile), "Backup saved")
            }
            FileDialog {
                id: importDialog
                fileMode: FileDialog.OpenFile
                nameFilters: ["SCANCORE backup (*.scancore)", "All files (*)"]
                onAccepted: page.backupTask = page.report(App.importBackup(selectedFile), (r) => r)
            }

            SectionTitle { text: "About" }
            SettingRow {
                visible: !Updater.managedByFlatpak
                title: "SCANCORE " + App.version
                description: Updater.state === "checking" ? "Checking for updates…"
                    : Updater.state === "current" ? "You have the latest version."
                    : Updater.state === "available" ? "Version " + Updater.version + " is available."
                    : Updater.state === "downloading" ? "Downloading " + Updater.version + " · " + Math.round(Updater.progress * 100) + "%"
                    : Updater.state === "ready" ? "Version " + Updater.version + " is ready to install."
                    : Updater.state === "error" ? Updater.error
                    : "Open source app by Medium Dev."
                Row {
                    spacing: 8
                    ScButton {
                        visible: Updater.state === "available" || Updater.state === "ready"
                        variant: "primary"
                        text: Updater.state === "ready" ? "Restart to update" : Updater.canInstall ? "Update" : "Download"
                        onClicked: Updater.canInstall ? (Updater.state === "ready" ? Updater.install() : Updater.download()) : App.openUrl(Updater.pageUrl)
                    }
                    ScButton {
                        text: "Check for updates"
                        loading: Updater.state === "checking"
                        enabled: Updater.state !== "downloading"
                        onClicked: Updater.check(false)
                    }
                }
            }
            SettingRow {
                visible: Updater.managedByFlatpak
                title: "SCANCORE " + App.version
                description: "Updates are delivered by Flatpak through your software center or system updater."
            }
            SettingRow {
                visible: !Updater.managedByFlatpak
                title: "Install updates automatically"
                description: Updater.canInstall ? "New versions download in the background and install when you restart."
                    : "This copy cannot update itself (for example a portable or source build); you will be told when a new version is out."
                Toggle { checked: App.settings.autoUpdate !== false; onToggled: App.setSetting("autoUpdate", checked) }
            }
            SettingRow {
                visible: !Updater.managedByFlatpak
                title: "Check for app updates"
                Toggle { checked: App.settings.checkAppUpdates !== false; onToggled: App.setSetting("checkAppUpdates", checked) }
            }
            SettingRow {
                title: "Source code"
                description: "github.com/medium-gh/scancore · MIT licence. Fonts: DM Sans, Space Grotesk, Literata (OFL). Icons: Lucide (ISC). HTML parsing: lexbor (Apache 2.0)."
                IconButton { iconName: "external-link"; onClicked: App.openUrl("https://github.com/medium-gh/scancore") }
            }
            SettingRow {
                title: "Welcome tour"
                description: "See the introduction again."
                ScButton { text: "Show"; onClicked: Router.shell.showWelcome() }
            }
            SettingRow {
                title: "Keyboard shortcuts"
                description: "Ctrl+K search · Ctrl+1–5 sections · Ctrl+B sidebar · Alt+Left back · F11 full screen. In readers: arrows, Space, N/P chapters, F full screen, M layout, +/− size, Esc close."
            }
        }
    }
}
