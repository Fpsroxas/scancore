#include "Platform.h"

#ifdef Q_OS_ANDROID
#include "core/Cookies.h"
#include "core/Http.h"
#include "core/Task.h"
#include "extensions/ExtensionSource.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJniEnvironment>
#include <QJniObject>
#include <QNetworkCookie>
#include <QUrl>
#endif

namespace sc::platform {

namespace {

std::function<void(const QString &)> g_siteCheckDone;

#ifdef Q_OS_ANDROID
constexpr auto kBrowserCheckClass = "com/mediumdev/scancore/BrowserCheck";
constexpr auto kPowerClass = "com/mediumdev/scancore/Power";
constexpr auto kNotifyClass = "com/mediumdev/scancore/Notify";

QString javaString(JNIEnv *env, jstring s) {
    if (!s)
        return {};
    const jchar *chars = env->GetStringChars(s, nullptr);
    QString out(reinterpret_cast<const QChar *>(chars), env->GetStringLength(s));
    env->ReleaseStringChars(s, chars);
    return out;
}

// "a=1; b=2" from Android's cookie store, for the site's host.
void jniSiteCheckDone(JNIEnv *env, jclass, jstring jurl, jstring jcookies) {
    const auto url = javaString(env, jurl);
    const auto header = javaString(env, jcookies);
    const QUrl site(url);
    QList<QNetworkCookie> cookies;
    // The web view keeps the real expiry; a month here is enough to carry
    // a check's pass (they last hours to days).
    const auto expires = QDateTime::currentDateTimeUtc().addDays(30);
    for (const auto &part : header.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const auto eq = part.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        QNetworkCookie cookie(part.left(eq).trimmed().toUtf8(), part.mid(eq + 1).trimmed().toUtf8());
        cookie.setDomain(QLatin1Char('.') + site.host());
        cookie.setPath(QStringLiteral("/"));
        cookie.setExpirationDate(expires);
        cookies.append(cookie);
    }
    if (!cookies.isEmpty()) {
        CookieJar::shared().setCookiesFromUrl(cookies, site);
        CookieJar::shared().save();
    }
    runInBackground([url] {
        try {
            ext::ExtensionSource::shareCookies(url);
        } catch (const Error &) {
            // The extension asks again if it still needs the check.
        }
    });
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [url] {
            if (g_siteCheckDone)
                g_siteCheckDone(url);
        },
        Qt::QueuedConnection);
}

bool registerBrowserCheck() {
    static const bool registered = [] {
        const JNINativeMethod methods[] = {
            {"nativeDone", "(Ljava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void *>(jniSiteCheckDone)},
        };
        QJniEnvironment env;
        return env.registerNativeMethods(kBrowserCheckClass, methods, 1);
    }();
    return registered;
}
#endif

} // namespace

void setSiteCheckDone(std::function<void(const QString &)> done) { g_siteCheckDone = std::move(done); }

bool openSiteCheck(const QString &url, const QString &title, const QString &doneLabel) {
#ifdef Q_OS_ANDROID
    if (!registerBrowserCheck())
        return false;
    QJniObject::callStaticMethod<void>(
        kBrowserCheckClass, "start", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QNativeInterface::QAndroidApplication::context().object(), QJniObject::fromString(url).object<jstring>(),
        QJniObject::fromString(QString::fromUtf8(Http::userAgent())).object<jstring>(),
        QJniObject::fromString(title).object<jstring>(), QJniObject::fromString(doneLabel).object<jstring>());
    return true;
#else
    Q_UNUSED(url)
    Q_UNUSED(title)
    Q_UNUSED(doneLabel)
    return false;
#endif
}

bool batteryRestricted() {
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>(kPowerClass, "restricted", "(Landroid/content/Context;)Z",
                                                  QNativeInterface::QAndroidApplication::context().object());
#else
    return false;
#endif
}

void askBatteryUnrestricted() {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(kPowerClass, "ask", "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context().object());
#endif
}

bool notify(const QString &title, const QString &body, const QString &tag) {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(
        kNotifyClass, "show", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QNativeInterface::QAndroidApplication::context().object(), QJniObject::fromString(title).object<jstring>(),
        QJniObject::fromString(body).object<jstring>(), QJniObject::fromString(tag).object<jstring>(),
        QJniObject::fromString(QCoreApplication::translate("Notifications", "Updates and announcements")).object<jstring>());
    return true;
#else
    Q_UNUSED(title)
    Q_UNUSED(body)
    Q_UNUSED(tag)
    return false;
#endif
}

void askNotificationPermission() {
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject::callStaticMethod<void>(kNotifyClass, "ask", "(Landroid/content/Context;)V",
                                           QNativeInterface::QAndroidApplication::context().object());
    });
#endif
}

} // namespace sc::platform
