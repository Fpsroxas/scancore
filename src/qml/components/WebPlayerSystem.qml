import QtQuick
import QtWebView
import ScanCore

// Player in the system's web view: Android WebView on phones, WebView2 on
// Windows and WKWebView on macOS. These play the H.264 episodes that the Qt
// WebEngine in Qt's own packages cannot. Like any native view it is drawn
// above everything Qt renders, so nothing can overlap it.
//
// Phones load the player inside a small page of ours, as WebPlayer does, and
// read its time through getTime/seek messages. The page reports back through
// its title, never through runJavaScript callbacks: on Android, Qt WebView
// runs those callbacks on the Android UI thread, alongside the QML engine on
// Qt's own thread, which crashes the app (a SIGSEGV in the JavaScript engine,
// often right after a tap). Title changes arrive as a signal, which Qt queues
// onto its own thread.
//
// Desktops open the player page itself and read its video element. WebView2
// cannot give a page of ours a real address, and inside a page without one
// the player cannot use its storage and never starts.
Item {
    id: root
    property string url
    readonly property bool direct: App.platform !== "android"
    signal time(real seconds, real total, string status)
    // The video was tapped (phones). Taps inside the cross-origin player never
    // reach the wrapper, but the first tap moves focus into the frame and
    // blurs the wrapper's window; handing focus back re-arms that for the next
    // tap.
    signal interacted()
    // Desktop: a key pressed while the page had focus, as the DOM tells it:
    // {key, ctrl, alt, shift, meta}.
    signal key(var press)
    // The player's own codec support, for parity with WebPlayer. System web
    // views play H.264, so this never fires.
    signal unsupported()
    property int taps: 0

    function seek(seconds) {
        if (direct)
            web.runJavaScript("(function(){var v=document.querySelector('video');if(v)v.currentTime=" + Number(seconds) + "})()")
        else
            web.runJavaScript("window.scSeek && window.scSeek(" + Number(seconds) + ")") // no callback: see above
    }

    function wrapper(src) {
        const safe = String(src).replace(/"/g, "&quot;").replace(/</g, "&lt;")
        return "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            + "<style>html,body{margin:0;height:100%;background:#000;overflow:hidden}iframe{border:0;width:100%;height:100%}</style>"
            + "</head><body><iframe id='f' src=\"" + safe + "\" allow='autoplay; fullscreen; encrypted-media' allowfullscreen referrerpolicy='origin'></iframe>"
            + "<script>"
            + "window.__sc={t:0,d:0,s:'',taps:0};var f=document.getElementById('f');"
            + "function pub(){var v='sc:'+JSON.stringify(__sc);if(document.title!==v)document.title=v}"
            + "addEventListener('message',function(e){if(e.source!==f.contentWindow)return;var x=e.data;"
            + "if(typeof x==='string'){try{x=JSON.parse(x)}catch(_){return}}if(!x)return;"
            + "if(x.currentTime!==undefined){__sc.t=+x.currentTime||0;__sc.d=+x.duration||0}"
            + "if(x.playerStatus)__sc.s=String(x.playerStatus);pub()});"
            + "setInterval(function(){try{f.contentWindow.postMessage({command:'getTime'},'*')}catch(_){}},1000);"
            + "window.scSeek=function(v){f.contentWindow.postMessage({command:'seek',value:v},'*')};"
            + "addEventListener('blur',function(){__sc.taps++;pub();setTimeout(function(){try{document.activeElement.blur();window.focus()}catch(_){}},200)});"
            + "</script></body></html>"
    }

    // Desktop: the video's state, plus the keys pressed since the last call.
    readonly property string probe: "(function(){var w=window;if(!w.__sc){w.__sc={k:[]};"
        + "w.addEventListener('keydown',function(e){if(e.repeat||/^(Control|Shift|Alt|Meta)$/.test(e.key))return;"
        + "w.__sc.k.push({key:e.key,ctrl:e.ctrlKey,alt:e.altKey,shift:e.shiftKey,meta:e.metaKey})},true)}"
        + "var v=document.querySelector('video'),k=w.__sc.k;w.__sc.k=[];"
        + "return JSON.stringify({t:v?v.currentTime:0,d:v&&isFinite(v.duration)?v.duration:0,s:v&&v.ended?'Ended':'',k:k})})()"

    function host(address) {
        return String(address).replace(/^[a-z]+:\/\//i, "").replace(/[\/?#].*$/, "").toLowerCase()
    }

    // Phones: video with sound, and the player's own full screen. The page
    // loads once the web view is prepared for them.
    property bool ready: direct
    Component.onCompleted: if (!direct) App.prepareWebPlayer()
    Connections {
        target: App
        enabled: !root.ready
        function onWebPlayerPrepared() { root.ready = true }
    }

    function load() {
        if (!url || !ready) return
        if (direct) web.url = url
        else web.loadHtml(wrapper(url), "https://reanime.to/")
    }
    onReadyChanged: load()
    onUrlChanged: {
        taps = 0
        load()
    }

    WebView {
        id: web
        anchors.fill: parent
        onTitleChanged: {
            if (root.direct) return
            const title = String(web.title)
            if (!title.startsWith("sc:")) return
            try {
                const s = JSON.parse(title.slice(3))
                if (s.taps > root.taps) {
                    root.taps = s.taps
                    root.interacted()
                }
                if (s.d > 0) root.time(s.t, s.d, s.s || "")
            } catch (e) {}
        }
        // Desktop: the player page stays put; an ad that navigates the page
        // away is sent back. (Pop-up windows are refused by the web view.)
        onLoadingChanged: (request) => {
            if (root.direct && root.url && request.status === WebView.LoadStartedStatus
                && root.host(request.url) !== root.host(root.url))
                web.url = root.url
        }
    }

    Timer {
        interval: 500
        running: root.direct && root.visible && root.url.length > 0
        repeat: true
        onTriggered: web.runJavaScript(root.probe, (result) => {
            try {
                const s = JSON.parse(result)
                for (const k of s.k || []) root.key(k)
                if (s.d > 0) root.time(s.t, s.d, s.s || "")
            } catch (e) {}
        })
    }
}
