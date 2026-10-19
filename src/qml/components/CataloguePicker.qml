import QtQuick
import ScanCore

// The tracker catalogue a page browses and searches, per media (settings
// "catalogComic" and "catalogNovel"). Comick has only comics; MyAnimeList
// needs SCANCORE's client id and is left out of builds without it.
Segmented {
    property string media: "comic"
    readonly property string key: media === "novel" ? "catalogNovel" : "catalogComic"

    options: (media === "comic" ? [{ value: "comick", label: qsTr("Comick") }] : [])
        .concat([{ value: "anilist", label: "AniList" }])
        .concat(Trackers.malAvailable ? [{ value: "mal", label: "MyAnimeList" }] : [])
        .concat([{ value: "none", label: qsTr("None") }])
    value: App.settings[key] || (media === "novel" ? "anilist" : "comick")
    onPicked: (v) => App.setSetting(key, v)
}
