#pragma once

namespace sc {

// Shares the embedded browser's cookies with the app's HTTP client, so a
// browser check passed in the verification window also lets SCANCORE's own
// requests through. The browser uses the client's user agent because
// Cloudflare ties clearance cookies to it. No-op without Qt WebEngine.
// Called by each embedded browser view as it is created (App.prepareWeb):
// the first call starts Chromium, about half a second, so not at start-up.
void installWebBridge();

} // namespace sc
