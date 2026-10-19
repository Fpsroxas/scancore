pragma Singleton
import QtQuick
import ScanCore

// Navigation entry points used by every page. Main.qml installs itself as
// the shell on startup.
QtObject {
    property var shell: null
    property string section: "home"

    function go(name) { if (shell) shell.goSection(name) }
    // A page on top of the current section's stack.
    function push(path, props) { if (shell) shell.push(path, props || {}) }
    function back() { if (shell) shell.back() }
    // A comic or book file: an archive, folder or PDF of pages, or an EPUB.
    function openFile(url) {
        const address = String(url)
        const name = decodeURIComponent(address.replace(/[?#].*$/, "").replace(/\/$/, "").replace(/^.*\//, ""))
        const media = /\.epub$/i.test(name) ? "novel" : "comic"
        openSeries({ media: media, sourceUrl: address, source: "local", title: name.replace(/\.[^.]+$/, "").replace(/_/g, " ") }, null)
    }
    // Settings, at one category.
    function openSettings(category) {
        App.setSetting("settingsCategory", category)
        go("settings")
    }
    // The "Add repository" sheet, for a link or (empty) to paste one.
    function addRepository(link) { if (shell) shell.addRepository(link || "") }
    // A repository link pasted into a search box opens the "Add repository"
    // sheet instead of searching for it. Returns whether it did.
    function takeRepositoryLink(text) {
        const t = String(text).trim()
        if (!/^(scancore|tachiyomi|mihon|aniyomi|lnreader):\/\//i.test(t)
                && !/^https?:\/\/\S+\/(index\.min\.json|repo\.json|plugins\.min\.json)$/i.test(t))
            return false
        addRepository(t)
        return true
    }
    // The library's categories, as anime or reading media call them.
    function libraryCategories(media) {
        const anime = media === "anime"
        return [{ value: "reading", label: anime ? qsTr("Watching") : qsTr("Reading") },
                { value: "planned", label: anime ? qsTr("Plan to watch") : qsTr("Plan to read") },
                { value: "paused", label: qsTr("On hold") },
                { value: "completed", label: qsTr("Completed") },
                { value: "dropped", label: qsTr("Dropped") }]
    }
    // One source's own popular, latest and filtered lists.
    function openSource(key) { push("pages/SourcePage.qml", { sourceKey: key }) }
    function openExtensions() { push("pages/ExtensionsPage.qml", {}) }
    function toast(message, kind) { if (shell) shell.toast(message, kind || "info") }

    // item: a search/library map with id, media, sourceUrl, title, cover.
    function openSeries(item, fromCover) {
        if (!shell || !item) return
        if (item.media === "anime")
            shell.push("pages/AnimeDetails.qml", { anilistId: item.anilistId || item.extra?.anilistId || 0, summary: item }, fromCover, item.cover)
        else if (item.media === "novel")
            shell.push("pages/NovelDetails.qml", { url: item.sourceUrl || item.url, summary: item }, fromCover, item.cover)
        else
            shell.push("pages/ComicDetails.qml", { url: item.sourceUrl || item.url, summary: item }, fromCover, item.cover)
    }

    function openComicReader(series, chapters, index) {
        if (shell) shell.openImmersive("pages/ComicReader.qml", { series: series, chapters: chapters, index: index })
    }

    function openNovelReader(series, chapters, index) {
        if (shell) shell.openImmersive("pages/NovelReader.qml", { series: series, chapters: chapters, index: index })
    }

    function openWatch(media, episodes, index) {
        if (shell) shell.openImmersive("pages/WatchPage.qml", { media: media, episodes: episodes, index: index })
    }

    // Lets the reader pass a site's browser check (see SiteCheck.qml).
    function openSiteCheck(url) {
        if (App.platform === "android") {
            // Android's own web view; its cookies come back to SCANCORE.
            const host = url.replace(/^https?:\/\//, "").replace(/\/.*$/, "")
            if (!App.openSiteCheck(url, qsTr("Browser check for %1. Complete any check the site shows, then press Done.").arg(host), qsTr("Done")))
                App.openUrl(url)
            return
        }
        if (shell && url) shell.openImmersive("pages/SiteCheck.qml", { url: url })
    }

    function openAnimeBrowse(filters) {
        if (shell) shell.push("pages/AnimeBrowse.qml", { initialFilters: filters || {} })
    }

    function openAnimeSchedule() {
        if (shell) shell.push("pages/AnimeSchedule.qml", {})
    }

    // For pages that refresh a list in the background: the old list when
    // the new one holds the same, so views keep their items (and scroll
    // position) instead of rebuilding them.
    function keep(old, fresh) {
        return JSON.stringify(old) === JSON.stringify(fresh) ? old : fresh
    }

    // A catalogue shelf's title by its key (Comics.catalogFeeds).
    function shelfTitle(key) {
        switch (key) {
        case "trending": return qsTr("Trending this week")
        case "popular": return qsTr("Most popular")
        case "top": return qsTr("Top rated")
        case "newest": return qsTr("New in the catalogue")
        case "completed": return qsTr("Recently completed")
        case "manhwa": return qsTr("Top manhwa")
        case "manhua": return qsTr("Top manhua")
        case "lightnovels": return qsTr("Top light novels")
        }
        return key
    }

    // Switches to a theme read from a .sct file (Customize.importTheme).
    function applyTheme(theme) {
        const t = Object.assign({}, theme)
        const accent = t.accent
        delete t.accent
        App.setSetting("customTheme", t)
        if (accent) {
            App.setSetting("customAccent", accent)
            App.setSetting("accent", "custom")
        }
        App.setSetting("theme", "custom")
        const name = t.name ? "“" + t.name + "”" : qsTr("The theme")
        if (t.windowOpacity < 1 && !Customize.translucentWindow && Customize.canRestart)
            toast(name + qsTr(" is on. Restart SCANCORE to make the window see-through."), "success")
        else
            toast(name + qsTr(" is on"), "success")
    }

    // Resume an item from history or the library at its last unit.
    function resume(entry) {
        if (!shell || !entry) return
        shell.resume(entry)
    }
}
