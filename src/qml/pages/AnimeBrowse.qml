import QtQuick
import QtQuick.Controls.Basic
import ScanCore

// AniList search with genre, year, format, status and sort filters.
Item {
    id: page
    property var initialFilters: ({})
    property string q: initialFilters.q || ""
    property var genres: initialFilters.genres || []
    property string format: ""
    property string status: ""
    property string sort: ""
    property int yearFrom: 0
    property int yearTo: 0
    property int pageNumber: 1
    property var results: []
    property bool hasNext: false
    property var task: null
    readonly property var years: [{ value: 0, label: "Any" }].concat(
        Array.from({ length: 50 }, (_, i) => ({ value: new Date().getFullYear() + 1 - i, label: String(new Date().getFullYear() + 1 - i) })))

    function run(reset) {
        if (reset) {
            pageNumber = 1
            results = []
        }
        task = Anime.search({ q: q, genres: genres, format: format, status: status, sort: sort,
                              yearFrom: yearFrom, yearTo: yearTo, page: pageNumber })
    }
    function search(text) {
        field.text = text
        q = text
        run(true)
    }

    Component.onCompleted: run(true)
    Connections {
        target: page.task
        function onFinished(result) {
            page.results = page.pageNumber === 1 ? result.results : page.results.concat(result.results)
            page.hasNext = result.hasNextPage
        }
    }
    Timer { id: debounce; interval: 450; onTriggered: page.run(true) }

    function loadMore() {
        if (!hasNext || !task || task.loading) return
        pageNumber += 1
        run(false)
    }

    SmoothFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height + 60
        onAtYEndChanged: if (atYEnd) page.loadMore()

        Column {
            id: column
            width: parent.width
            spacing: 20

            PageHeader {
                title: "Browse anime"
                subtitle: "Search AniList by title, genre, year and more"
                showBack: true
            }

            Flow {
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 10
                SearchField {
                    id: field
                    width: Math.min(360, parent.width)
                    text: page.q
                    placeholderText: "Search titles"
                    onTextChanged: if (text !== page.q) { page.q = text; debounce.restart() }
                }
                Select {
                    model: [{ value: "", label: "Best match" }, { value: "POPULARITY_DESC", label: "Most popular" },
                            { value: "TRENDING_DESC", label: "Trending" }, { value: "SCORE_DESC", label: "Highest rated" },
                            { value: "START_DATE_DESC", label: "Newest" }, { value: "FAVOURITES_DESC", label: "Most favourited" }]
                    value: page.sort
                    onPicked: (v) => { page.sort = v; page.run(true) }
                }
                Select {
                    model: [{ value: "", label: "Any format" }, { value: "TV", label: "TV" }, { value: "MOVIE", label: "Movie" },
                            { value: "OVA", label: "OVA" }, { value: "ONA", label: "ONA" }, { value: "SPECIAL", label: "Special" },
                            { value: "TV_SHORT", label: "TV Short" }]
                    value: page.format
                    onPicked: (v) => { page.format = v; page.run(true) }
                }
                Select {
                    model: [{ value: "", label: "Any status" }, { value: "RELEASING", label: "Airing" },
                            { value: "FINISHED", label: "Finished" }, { value: "NOT_YET_RELEASED", label: "Upcoming" }]
                    value: page.status
                    onPicked: (v) => { page.status = v; page.run(true) }
                }
                Select {
                    implicitWidth: 120
                    model: page.years
                    value: page.yearFrom
                    displayText: page.yearFrom ? "From " + page.yearFrom : "From any year"
                    onPicked: (v) => { page.yearFrom = v; page.run(true) }
                }
                Select {
                    implicitWidth: 120
                    model: page.years
                    value: page.yearTo
                    displayText: page.yearTo ? "To " + page.yearTo : "To any year"
                    onPicked: (v) => { page.yearTo = v; page.run(true) }
                }
            }

            Flow {
                x: Theme.gutter
                width: parent.width - 2 * Theme.gutter
                spacing: 8
                Repeater {
                    model: Anime.genres()
                    Chip {
                        required property string modelData
                        text: modelData
                        selected: page.genres.indexOf(modelData) >= 0
                        onClicked: {
                            const g = page.genres.slice()
                            const i = g.indexOf(modelData)
                            if (i >= 0) g.splice(i, 1)
                            else g.push(modelData)
                            page.genres = g
                            page.run(true)
                        }
                    }
                }
            }

            CoverGrid {
                items: page.results
                subtitleFor: (it) => [it.format, it.year, it.score ? it.score + "%" : ""].filter(Boolean).join(" · ")
                onOpened: (it, cover) => Router.openSeries(it, cover)
            }

            // When the results do not fill the page there is no scrolling to
            // reach the end with.
            ScButton {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.hasNext && page.task !== null && page.task.ok
                text: "Load more"
                onClicked: page.loadMore()
            }

            StateView {
                width: parent.width
                height: visible ? (page.results.length ? 90 : 300) : 0
                task: page.task
                compact: page.results.length > 0
                empty: page.task && page.task.ok && page.results.length === 0
                emptyTitle: "No anime match these filters"
                emptyIcon: "funnel"
                onRetry: page.run(page.pageNumber === 1)
            }
        }
    }
}
