#include "App.h"

#include "WebBridge.h"
#include "Images.h"
#include "Announcements.h"
#include "Library.h"
#include "Notifications.h"
#include "Platform.h"
#include "Profiler.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/ImageCache.h"
#include "extensions/Manager.h"
#include "sources/Manifest.h"
#include "sources/comics/Registry.h"
#include "sources/novels/Registry.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QQuickWindow>
#include <QStandardPaths>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#else
#include <QSystemTrayIcon>
#endif
#include <QVersionNumber>

namespace sc {

namespace {

const QVariantMap kDefaults{
    {"theme", "dark"},
    {"accent", "mono"},
    {"reducedMotion", false},
    {"readerMode", "strip"},
    {"readerWidth", 820},
    {"readerGap", 0},
    {"readerDirection", "ltr"},
    {"readerSpread", "single"},
    {"readerCrop", false},
    {"readerProgressBar", true},
    // Readers open in full screen (their full screen button sets it).
    {"readerFullscreen", false},
    {"novelFont", "Literata"},
    {"novelSize", 19},
    {"novelLineHeight", 1.5},
    {"novelWidth", 720},
    {"novelTheme", "app"},
    {"novelJustify", false},
    // More of the novel reader's typography and look.
    {"novelWeight", 400},
    {"novelParagraphSpacing", 0.9},
    {"novelIndent", false},
    {"novelMargin", 32},
    {"novelDim", 0},
    {"novelCustomBg", "#1b1a17"},
    {"novelCustomText", "#e6dfd2"},
    {"novelAutoScrollSpeed", 40},
    // Reading aloud (Speech): voice id, speed and pitch multipliers, go on
    // to the next chapter, keep the spoken paragraph on screen.
    {"ttsVoice", ""},
    {"ttsRate", 1},
    {"ttsPitch", 1},
    {"ttsContinue", true},
    {"ttsFollow", true},
    {"autoNext", true},
    // The video player's volume (0-130) and speed, kept between episodes.
    {"playerVolume", 100},
    {"playerSpeed", 1},
    {"autoSkip", false},
    {"libraryCheckHours", 2},
    {"notifyUpdates", true},
    {"showNsfwSources", false},
    {"imageCacheMB", 1024},
    {"closeToTray", false},
    // Android: the one-time question about battery optimisation was asked.
    {"batteryAsked", false},
    {"notificationsAsked", false},
    // Announcements from scancore.onl in the notification area.
    {"announcements", true},
    {"checkAppUpdates", true},
    {"sidebarCollapsed", false},
    {"comicSource", "all"},
    // The library: which media, category ("all", "favourites" or one of
    // Router.libraryCategories) and view (grid, compact, list).
    {"libraryMedia", "comic"},
    {"libraryCategory", "all"},
    {"libraryView", "grid"},
    {"librarySort", "updates"},
    {"comickCatalog", true},
    // The tracker catalogue each page browses and searches: comick,
    // anilist, mal or none (novels: anilist, mal or none).
    {"catalogComic", "comick"},
    {"catalogNovel", "anilist"},
    // Anime: anilist or mal (the Anime page's shelves).
    {"catalogAnime", "anilist"},
    // Content language "All sources" searches; empty for every language.
    {"sourceLanguages", ""},
    {"onboarded", false},
    {"autoUpdate", true},
    // Extension updates signed with a trusted key install by themselves.
    {"autoUpdateExtensions", true},
    {"scrollSpeed", 1},
    {"novelSource", "all"},
};

} // namespace

App::App(QObject *parent) : QObject(parent) {
    m_settings = kDefaults;
    for (auto it = kDefaults.begin(); it != kDefaults.end(); ++it)
        m_settings.insert(it.key(), Database::pref(QStringLiteral("setting.") + it.key(), it.value()));
    // Before 2.0 the only catalogue was Comick's, switched by comickCatalog.
    if (!Database::pref(QStringLiteral("setting.catalogComic")).isValid() && m_settings.value("comickCatalog") == false)
        m_settings.insert(QStringLiteral("catalogComic"), QStringLiteral("none"));
    m_libraryTimer.setSingleShot(false);
    connect(&m_libraryTimer, &QTimer::timeout, this, [] { Library::instance()->checkUpdates(); });
    platform::setSiteCheckDone([this](const QString &url) { emit siteChecked(url); });
#ifdef Q_OS_ANDROID
    // The reader may change it in the system's settings while away.
    m_batteryRestricted = platform::batteryRestricted();
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive)
            return;
        const bool restricted = platform::batteryRestricted();
        if (restricted != m_batteryRestricted) {
            m_batteryRestricted = restricted;
            emit batteryChanged();
        }
    });
#endif
}

App *App::instance() {
    static auto *app = [] {
        auto *created = new App;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return app;
}

QString App::version() const { return QCoreApplication::applicationVersion(); }

QString App::platform() const {
#if defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

QString App::dataFolder() const { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }
int App::manifestVersion() const { return Manifest::version(); }

bool App::webEngine() const {
#if defined(SCANCORE_HAVE_WEBENGINE) || defined(SCANCORE_HAVE_WEBVIEW)
    return true;
#else
    return false;
#endif
}

void App::prepareWeb() const { installWebBridge(); }

QVariantMap App::startAction() const {
    return QJsonDocument::fromJson(qgetenv("SCANCORE_START")).object().toVariantMap();
}

void App::setSetting(const QString &key, const QVariant &value) {
    if (m_settings.value(key) == value)
        return;
    m_settings.insert(key, value);
    Database::setPref(QStringLiteral("setting.") + key, value);
    if (key == QLatin1String("libraryCheckHours"))
        scheduleLibraryCheck();
    if (key == QLatin1String("announcements")) {
        if (value.toBool())
            QMetaObject::invokeMethod(this, [] { Announcements::instance()->start(); }, Qt::QueuedConnection);
        else
            Announcements::instance()->stop();
    }
    if (key == QLatin1String("imageCacheMB")) {
        ImageCache::setBudget(std::max<qint64>(128, value.toLongLong()) * 1024 * 1024);
        // A smaller limit takes effect now, not at the next start.
        runInBackground([] { ImageCache::trim(); });
    }
    emit settingsChanged();
}

QString App::image(const QString &url, const QString &referer) const {
    if (url.isEmpty())
        return {};
    if (url.startsWith(QLatin1String("file:")) || url.startsWith(QLatin1String("qrc:")) ||
        url.startsWith(QLatin1String("image:")))
        return url;
    return RemoteImageProvider::encode(url, referer);
}

QString App::icon(const QString &name, const QString &color) const {
    return QStringLiteral("image://icon/%1?color=%2").arg(name, QString::fromUtf8(QUrl::toPercentEncoding(color)));
}

void App::openUrl(const QString &url) const {
    const QUrl parsed(url);
    if (parsed.scheme() == QLatin1String("https") || parsed.scheme() == QLatin1String("http"))
        QDesktopServices::openUrl(parsed);
}

void App::moveToBackground() const {
#ifdef Q_OS_ANDROID
    // What Back on an app's first screen does on Android 12 and later. Runs on
    // the Android UI thread without waiting for it, so it cannot deadlock with
    // Qt's thread.
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
            activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", jboolean(true));
    });
#endif
}

bool App::openSiteCheck(const QString &url, const QString &title, const QString &doneLabel) {
    return platform::openSiteCheck(url, title, doneLabel);
}

void App::askBatteryUnrestricted() const { platform::askBatteryUnrestricted(); }

bool App::profiling() const { return Profiler::enabled(); }

void App::profilePlace(const QString &place) const { Profiler::instance()->setPlace(place); }

void App::openFolder(const QString &path) const {
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void App::copy(const QString &text) const { QGuiApplication::clipboard()->setText(text); }

void App::notify(const QString &title, const QString &body) {
#ifndef Q_OS_ANDROID
    if (m_tray && QSystemTrayIcon::supportsMessages())
        m_tray->showMessage(title, body, QSystemTrayIcon::Information, 6000);
#else
    Q_UNUSED(title)
    Q_UNUSED(body)
#endif
}

QString App::relativeTime(const QVariant &when) const {
    QDateTime time;
    if (when.typeId() == QMetaType::QString)
        time = QDateTime::fromString(when.toString(), Qt::ISODate);
    else if (when.toLongLong() > 0)
        time = QDateTime::fromSecsSinceEpoch(when.toLongLong());
    if (!time.isValid())
        return {};
    const auto secs = time.secsTo(QDateTime::currentDateTime());
    if (secs < 0) {
        const auto ahead = -secs;
        if (ahead < 3600)
            return QStringLiteral("in %1m").arg(std::max<qint64>(1, ahead / 60));
        if (ahead < 86400)
            return QStringLiteral("in %1h").arg(ahead / 3600);
        return QStringLiteral("in %1d").arg(ahead / 86400);
    }
    if (secs < 60)
        return QStringLiteral("just now");
    if (secs < 3600)
        return QStringLiteral("%1m ago").arg(secs / 60);
    if (secs < 86400)
        return QStringLiteral("%1h ago").arg(secs / 3600);
    if (secs < 7 * 86400)
        return QStringLiteral("%1d ago").arg(secs / 86400);
    return QLocale().toString(time.date(), QLocale::ShortFormat);
}

QString App::formatBytes(qint64 bytes) const { return QLocale().formattedDataSize(bytes); }

bool App::saveWindowImage(QObject *window, const QString &path) const {
    auto *quick = qobject_cast<QQuickWindow *>(window);
    return quick && quick->grabWindow().save(path);
}

Task *App::checkForUpdate() {
    return Task::run([this]() -> QVariant {
        const auto doc = Http::getJson(QUrl(QStringLiteral("https://api.github.com/repos/medium-gh/scancore/releases/latest")),
                                       {{"Accept", "application/vnd.github+json"}});
        const auto release = doc.object();
        auto tag = release.value("tag_name").toString();
        if (tag.startsWith(QLatin1Char('v')))
            tag.remove(0, 1);
        const auto latest = QVersionNumber::fromString(tag);
        qsizetype suffix = 0;
        const auto running = QCoreApplication::applicationVersion();
        const auto current = QVersionNumber::fromString(running, &suffix);
        // A pre-release (2.0.0-beta.1) is older than its release.
        const bool available = !latest.isNull() && (latest > current || (latest == current && suffix < running.size()));
        const auto url = release.value("html_url").toString();
        if (available)
            QMetaObject::invokeMethod(this, [this, tag, url] { emit updateAvailable(tag, url); });
        return QVariantMap{{"available", available},
                           {"version", tag},
                           {"url", url},
                           {"notes", release.value("body").toString()}};
    });
}

Task *App::refreshSources() {
    return Task::run([this]() -> QVariant {
        const bool changed = Manifest::refresh();
        if (changed) {
            comics::Registry::rebuild();
            novels::Registry::rebuild();
            QMetaObject::invokeMethod(this, [this] { emit manifestChanged(); });
        }
        return QVariantMap{{"changed", changed}, {"version", Manifest::version()}};
    });
}

Task *App::cacheSize() {
    return Task::run([]() -> QVariant { return ImageCache::sizeOnDisk(); });
}

void App::clearCache() {
    runInBackground([] {
        ImageCache::clear();
        Database::exec(QStringLiteral("DELETE FROM cache"));
    });
}

Task *App::exportBackup(const QUrl &file) {
    return Task::run([file]() -> QVariant {
        const auto path = file.toLocalFile();
        if (path.isEmpty())
            throw Error(QStringLiteral("Choose a file to save the backup to."));
        QFile::remove(path);
        // VACUUM INTO writes a consistent copy while the app keeps running.
        auto escaped = path;
        escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
        Database::exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped));
        return path;
    });
}

Task *App::importBackup(const QUrl &file) {
    return Task::run([file]() -> QVariant {
        const auto path = file.toLocalFile();
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly) || in.read(16) != QByteArray("SQLite format 3\0", 16))
            throw Error(QStringLiteral("That file is not a SCANCORE backup."));
        in.close();
        const auto pending = Database::path() + QStringLiteral(".restore");
        QFile::remove(pending);
        if (!QFile::copy(path, pending))
            throw Error(QStringLiteral("Could not copy the backup."));
        return QStringLiteral("Restart SCANCORE to finish restoring the backup.");
    });
}

void App::scheduleLibraryCheck() {
    const int hours = m_settings.value(QStringLiteral("libraryCheckHours")).toInt();
    if (hours <= 0) {
        m_libraryTimer.stop();
        return;
    }
    m_libraryTimer.start(hours * 3600 * 1000);
}

void App::startBackgroundWork() {
    // New chapters reach the notification area (and the system's
    // notifications) from Library::checkUpdates itself; a new SCANCORE
    // from here.
    connect(this, &App::updateAvailable, this, [](const QString &version, const QString &url) {
        Notifications::post({QStringLiteral("app"), QStringLiteral("app:") + version, tr("SCANCORE %1 is available").arg(version),
                             tr("See what is new and update."), QVariantMap{{"url", url}}, true});
    });
    // Announcements from scancore.onl, live while online.
    QTimer::singleShot(6000, this, [] { Announcements::instance()->start(); });
    scheduleLibraryCheck();
    QTimer::singleShot(4000, this, [this] {
        refreshSources();
        if (m_settings.value(QStringLiteral("libraryCheckHours")).toInt() > 0)
            Library::instance()->checkUpdates();
        runInBackground([] {
            ImageCache::trim();
            Database::cachePrune();
        });
    });
    // Repositories are read again twice a day (a conditional request each,
    // so usually a few bytes), and trusted updates installed.
    const auto checkExtensions = [this] {
        const bool install = m_settings.value(QStringLiteral("autoUpdateExtensions")).toBool();
        runInBackground([install] {
            ext::Manager::refreshAll();
            if (install)
                ext::Manager::installTrustedUpdates();
            // What is still waiting (new signing keys, or automatic
            // updates off).
            const int waiting = int(ext::Manager::updates().size());
            if (waiting > 0)
                Notifications::post({QStringLiteral("extensions"), QStringLiteral("extensions:updates"),
                                     waiting == 1 ? tr("An extension update is ready") : tr("%1 extension updates are ready").arg(waiting),
                                     tr("Open Extensions to update them."), QVariantMap{{"page", "extensions"}}, false});
        });
    };
    QTimer::singleShot(20000, this, checkExtensions);
    auto *extensionTimer = new QTimer(this);
    extensionTimer->setInterval(12 * 3600 * 1000);
    connect(extensionTimer, &QTimer::timeout, this, checkExtensions);
    extensionTimer->start();
}

} // namespace sc
