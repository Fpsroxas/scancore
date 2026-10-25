pragma Singleton
import QtQuick
QtObject {
    property var opened: []
    function keep(old, result) { return result }
    function takeRepositoryLink(text) { return false }
    function openSeries(item, cover) { opened = opened.concat([item]) }
    function openSource() { throw new Error("Source selection must stay inline") }
    function shelfTitle(key) { return key }
}
