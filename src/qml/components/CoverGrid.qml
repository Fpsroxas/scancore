import QtQuick
import ScanCore

// Responsive grid of cover cards for use inside a scrolling page.
Item {
    id: grid
    property var items: []
    property int minCardWidth: Math.round((Theme.compact ? 104 : 150) * Theme.coverScale)
    property int spacing: Theme.compact ? 12 : 20
    property real coverRatio: 1.5
    property var subtitleFor: (item) => ""
    property var badgeFor: (item) => ""
    property var progressFor: (item) => -1
    signal opened(var item, Item coverItem)

    // Covers rise in when the grid first fills; refreshes and filtering after
    // that show them in place instead of replaying the entrance.
    property bool settled: false
    onItemsChanged: if (items.length > 0 && !settled) Qt.callLater(() => grid.settled = true)

    readonly property int columns: Math.max(2, Math.floor((width + spacing) / (minCardWidth + spacing)))
    readonly property real cardWidth: (width - (columns - 1) * spacing) / columns

    width: parent ? parent.width - 2 * Theme.gutter : 800
    x: Theme.gutter
    height: flow.height

    Flow {
        id: flow
        width: parent.width
        spacing: grid.spacing
        Repeater {
            model: grid.items
            delegate: CoverCard {
                required property var modelData
                required property int index
                width: grid.cardWidth
                item: modelData
                index: index
                animateIn: !grid.settled
                coverRatio: grid.coverRatio
                subtitle: grid.subtitleFor(modelData)
                badge: grid.badgeFor(modelData)
                progress: grid.progressFor(modelData)
                onOpened: (it, cover) => grid.opened(it, cover)
            }
        }
    }
}
