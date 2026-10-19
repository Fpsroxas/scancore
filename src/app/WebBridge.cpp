#include "WebBridge.h"

#ifdef SCANCORE_HAVE_WEBENGINE
#include "core/Cookies.h"
#include "core/Http.h"

#include <QNetworkCookie>
#include <QWebEngineCookieStore>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#endif

namespace sc {

void installWebBridge() {
#ifdef SCANCORE_HAVE_WEBENGINE
    static bool installed = false;
    if (installed)
        return;
    installed = true;
    auto *profile = QQuickWebEngineProfile::defaultProfile();
    if (!profile)
        return;
    profile->setHttpUserAgent(QString::fromUtf8(Http::userAgent()));
    auto *store = profile->cookieStore();
    QObject::connect(store, &QWebEngineCookieStore::cookieAdded, &CookieJar::shared(), [](const QNetworkCookie &cookie) {
        CookieJar::shared().insertCookie(cookie);
        CookieJar::shared().save();
    });
    QObject::connect(store, &QWebEngineCookieStore::cookieRemoved, &CookieJar::shared(),
                     [](const QNetworkCookie &cookie) { CookieJar::shared().deleteCookie(cookie); });
    store->loadAllCookies();
#endif
}

} // namespace sc
