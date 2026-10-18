import QtQuick
import QtQuick.Dialogs
import ScanCore

SettingsPane {
    id: pane
    title: qsTr("Sources")
    subtitle: qsTr("The sites SCANCORE reads from")
    iconName: "globe"

    property var refreshTask: null
    readonly property var comicSources: Comics.sources()
    readonly property var novelSources: { Extensions.novelPackInstalled; return Novels.sources().filter(s => !s.extended) }
    readonly property int extendedCount: { Extensions.novelPackInstalled; return Novels.sources().filter(s => s.extended).length }

    function refresh() {
        refreshTask = App.refreshSources()
        refreshTask.finished.connect((r) => Router.toast(r.changed ? qsTr("Sources updated to version ") + r.version : qsTr("Sources are up to date"), "success"))
        refreshTask.failed.connect((error) => Router.toast(error, "error"))
    }

    component SourceRow: SettingRow {
        required property var modelData
        title: modelData.label + (modelData.nsfw ? "  · 18+" : "")
        description: modelData.base.replace("https://", "") + (modelData.enabled ? "" : " · " + modelData.disabledReason)
        Row {
            spacing: 4
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 8
                height: 8
                radius: 4
                color: modelData.enabled ? Theme.success : Theme.textFaint
            }
            Item { width: 6; height: 1 }
            IconButton {
                visible: App.platform !== "android"
                iconName: "shield-check"
                tip: qsTr("Pass a browser check for this site")
                onClicked: Router.openSiteCheck(modelData.base + "/")
            }
            IconButton { iconName: "external-link"; tip: qsTr("Open site"); onClicked: App.openUrl(modelData.base) }
        }
    }

    // Comics and books on this device: the folders listed and searched.
    readonly property var folders: Array.from(App.settings.localFolders || [])
    SettingsGroup {
        title: qsTr("On this device")
        note: qsTr("Each folder or file in these folders is a series: folders of images, CBZ, CBR, 7z and PDF files for comics, and EPUB books for novels. ")
            + qsTr("Comics and Novels list them under On this device. CBR and 7z files need 7-Zip, unrar or bsdtar installed.")
        Repeater {
            model: pane.folders
            SettingRow {
                required property string modelData
                iconName: "folder-open"
                title: modelData.replace(/^.*[\/\\]/, "") || modelData
                description: modelData
                IconButton {
                    iconName: "x"
                    tip: qsTr("Stop listing this folder")
                    onClicked: App.setSetting("localFolders", pane.folders.filter(f => f !== modelData))
                }
            }
        }
        SettingRow {
            iconName: "plus"
            title: pane.folders.length ? qsTr("Add another folder") : qsTr("Add a folder")
            description: qsTr("Nothing is copied: SCANCORE reads the files where they are.")
            Row {
                spacing: 8
                ScButton { text: qsTr("Add folder"); iconName: "folder-open"; onClicked: localFolderDialog.open() }
                ScButton { text: qsTr("Open a file"); variant: "ghost"; onClicked: localFileDialog.open() }
            }
        }
    }
    FolderDialog {
        id: localFolderDialog
        onAccepted: {
            const path = App.localPath(selectedFolder)
            if (pane.folders.indexOf(path) < 0) App.setSetting("localFolders", pane.folders.concat([path]))
        }
    }
    FileDialog {
        id: localFileDialog
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Comics and books (*.cbz *.zip *.cbr *.rar *.cb7 *.7z *.pdf *.epub)"), qsTr("All files (*)")]
        onAccepted: Router.openFile(selectedFile)
    }

    SettingsGroup {
        title: qsTr("Source list")
        note: qsTr("Site addresses and availability come from the SCANCORE repository, so a moved or broken site can be fixed without a new release.")
        SettingRow {
            iconName: "refresh-cw"
            title: qsTr("Version ") + App.manifestVersion
            description: pane.comicSources.length + qsTr(" comic and ") + pane.novelSources.length + qsTr(" novel sources built in.")
            ScButton {
                text: qsTr("Check now")
                loading: pane.refreshTask ? pane.refreshTask.loading : false
                onClicked: pane.refresh()
            }
        }
        SettingRow {
            iconName: "search"
            title: qsTr("Search with Comick's catalogue")
            description: qsTr("“All sources” searches comick.dev, which knows almost every series under all its names, then opens ")
                + qsTr("the one you pick from a source that has it. Off: each source is searched directly.")
            Toggle { checked: App.settings.comickCatalog !== false; onToggled: App.setSetting("comickCatalog", checked) }
        }
        SettingRow {
            iconName: "eye"
            title: qsTr("Show adult sources")
            description: qsTr("Lists sources marked 18+ in the comic source picker.")
            Toggle { checked: App.settings.showNsfwSources === true; onToggled: App.setSetting("showNsfwSources", checked) }
        }
    }

    SettingsGroup {
        visible: App.platform !== "android"
        title: qsTr("Extended novel sources")
        SettingRow {
            iconName: "book-text"
            title: !Extensions.novelPackAvailable ? qsTr("Not available in this build")
                : Extensions.novelPackInstalled ? pane.extendedCount + qsTr(" more novel sites installed")
                : qsTr("About 260 more novel sites")
            description: !Extensions.novelPackAvailable ? qsTr("The downloadable Python novel-source pack is turned off in this build.")
                : Extensions.busy ? Extensions.status
                : Extensions.novelPackInstalled ? "From lightnovel-crawler. Pick them from “More sources” on the Novels page; pasted book links from any of them open directly."
                : qsTr("Uses lightnovel-crawler. Downloads a private copy of Python and the crawler (about 400 MB) into the app data folder.")
                  + (Extensions.status ? qsTr(" Last attempt: ") + Extensions.status : "")
            Row {
                spacing: 8
                ScButton {
                    visible: Extensions.novelPackAvailable && !Extensions.novelPackInstalled
                    variant: "primary"
                    text: qsTr("Install")
                    iconName: "download"
                    loading: Extensions.busy
                    enabled: !Extensions.busy
                    onClicked: {
                        const task = Extensions.installNovelPack()
                        task.finished.connect(() => Router.toast(qsTr("Extended novel sources installed"), "success"))
                        task.failed.connect((error) => Router.toast(error, "error"))
                    }
                }
                ScButton {
                    visible: Extensions.novelPackAvailable && Extensions.novelPackInstalled
                    text: qsTr("Remove")
                    variant: "ghost"
                    onClicked: Extensions.removeNovelPack()
                }
            }
        }
    }

    SettingsGroup {
        title: qsTr("Comics")
        note: App.platform !== "android" ? "If a site asks for a browser check, the shield opens it inside SCANCORE once; the app then reuses the clearance." : ""
        Repeater {
            model: pane.comicSources
            SourceRow {}
        }
    }

    SettingsGroup {
        title: qsTr("Novels")
        Repeater {
            model: pane.novelSources
            SourceRow {}
        }
    }
}
