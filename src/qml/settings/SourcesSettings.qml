import QtQuick
import ScanCore

SettingsPane {
    id: pane
    title: "Sources"
    subtitle: "The sites SCANCORE reads from"
    iconName: "globe"

    property var refreshTask: null
    readonly property var comicSources: Comics.sources()
    readonly property var novelSources: { Extensions.novelPackInstalled; return Novels.sources().filter(s => !s.extended) }
    readonly property int extendedCount: { Extensions.novelPackInstalled; return Novels.sources().filter(s => s.extended).length }

    function refresh() {
        refreshTask = App.refreshSources()
        refreshTask.finished.connect((r) => Router.toast(r.changed ? "Sources updated to version " + r.version : "Sources are up to date", "success"))
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
                tip: "Pass a browser check for this site"
                onClicked: Router.openSiteCheck(modelData.base + "/")
            }
            IconButton { iconName: "external-link"; tip: "Open site"; onClicked: App.openUrl(modelData.base) }
        }
    }

    SettingsGroup {
        title: "Source list"
        note: "Site addresses and availability come from the SCANCORE repository, so a moved or broken site can be fixed without a new release."
        SettingRow {
            iconName: "refresh-cw"
            title: "Version " + App.manifestVersion
            description: pane.comicSources.length + " comic and " + pane.novelSources.length + " novel sources built in."
            ScButton {
                text: "Check now"
                loading: pane.refreshTask ? pane.refreshTask.loading : false
                onClicked: pane.refresh()
            }
        }
        SettingRow {
            iconName: "eye"
            title: "Show adult sources"
            description: "Lists sources marked 18+ in the comic source picker."
            Toggle { checked: App.settings.showNsfwSources === true; onToggled: App.setSetting("showNsfwSources", checked) }
        }
    }

    SettingsGroup {
        visible: App.platform !== "android"
        title: "Extended novel sources"
        SettingRow {
            iconName: "book-text"
            title: !Extensions.novelPackAvailable ? "Not available in this build"
                : Extensions.novelPackInstalled ? pane.extendedCount + " more novel sites installed"
                : "About 260 more novel sites"
            description: !Extensions.novelPackAvailable ? "The downloadable Python novel-source pack is turned off in this build."
                : Extensions.busy ? Extensions.status
                : Extensions.novelPackInstalled ? "From lightnovel-crawler. Pick them from “More sources” on the Novels page; pasted book links from any of them open directly."
                : "Uses lightnovel-crawler. Downloads a private copy of Python and the crawler (about 400 MB) into the app data folder."
                  + (Extensions.status ? " Last attempt: " + Extensions.status : "")
            Row {
                spacing: 8
                ScButton {
                    visible: Extensions.novelPackAvailable && !Extensions.novelPackInstalled
                    variant: "primary"
                    text: "Install"
                    iconName: "download"
                    loading: Extensions.busy
                    enabled: !Extensions.busy
                    onClicked: {
                        const task = Extensions.installNovelPack()
                        task.finished.connect(() => Router.toast("Extended novel sources installed", "success"))
                        task.failed.connect((error) => Router.toast(error, "error"))
                    }
                }
                ScButton {
                    visible: Extensions.novelPackAvailable && Extensions.novelPackInstalled
                    text: "Remove"
                    variant: "ghost"
                    onClicked: Extensions.removeNovelPack()
                }
            }
        }
    }

    SettingsGroup {
        title: "Comics"
        note: App.platform !== "android" ? "If a site asks for a browser check, the shield opens it inside SCANCORE once; the app then reuses the clearance." : ""
        Repeater {
            model: pane.comicSources
            SourceRow {}
        }
    }

    SettingsGroup {
        title: "Novels"
        Repeater {
            model: pane.novelSources
            SourceRow {}
        }
    }
}
