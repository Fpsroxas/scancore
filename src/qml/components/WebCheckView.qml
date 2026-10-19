import QtQuick
import QtWebEngine
import ScanCore

// A plain browser view for passing a site's browser check. Cookies it
// receives are shared with SCANCORE's own requests (see WebBridge).
WebEngineView {
    id: view
    property string startUrl
    backgroundColor: "white"
    url: startUrl
    settings.javascriptCanOpenWindows: false
    onNewWindowRequested: (request) => { /* stay in this view */ }
    // Before startUrl is set: shares cookies and the user agent.
    Component.onCompleted: App.prepareWeb()
}
