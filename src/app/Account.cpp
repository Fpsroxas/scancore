#include "Account.h"

#include "Library.h"
#include "core/Cookies.h"
#include "core/Database.h"
#include "sources/comics/Registry.h"
#include "sources/novels/Registry.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QRegularExpression>
#include <QTimer>

namespace sc {

namespace {

const QString kSite = QStringLiteral("https://scancore.onl");

Headers apiHeaders() {
    return {{"Accept", "application/json"}, {"Content-Type", "application/json"}};
}

QString categoryFor(const QString &status) {
    if (status == QLatin1String("COMPLETED"))
        return QStringLiteral("completed");
    if (status == QLatin1String("DROPPED"))
        return QStringLiteral("dropped");
    if (status == QLatin1String("PLANNED"))
        return QStringLiteral("planned");
    return QStringLiteral("reading");
}

QString statusFor(const QString &category) {
    if (category == QLatin1String("completed"))
        return QStringLiteral("COMPLETED");
    if (category == QLatin1String("dropped"))
        return QStringLiteral("DROPPED");
    if (category == QLatin1String("planned"))
        return QStringLiteral("PLANNED");
    return QStringLiteral("IN_PROGRESS");
}

// Maps a website series to the local series id, or an empty map when the
// desktop app has no matching source.
QVariantMap localSeries(const QJsonObject &series) {
    const auto media = series.value("mediaType").toString();
    QVariantMap out{{"title", series.value("title").toString()},
                    {"cover", series.value("cover").toString()},
                    {"description", series.value("description").toString()}};
    for (const auto &value : series.value("sourceMappings").toArray()) {
        const auto mapping = value.toObject();
        const auto name = mapping.value("sourceName").toString();
        const auto url = mapping.value("canonicalUrl").toString();
        if (media == QLatin1String("ANIME") && name.startsWith(QLatin1String("anime:anilist"))) {
            const auto id = mapping.value("externalId").toString();
            out.insert("id", Library::idFor(QStringLiteral("anime"), QStringLiteral("anilist"), id));
            out.insert("media", QStringLiteral("anime"));
            out.insert("source", QStringLiteral("anilist"));
            out.insert("sourceUrl", QStringLiteral("https://anilist.co/anime/%1").arg(id));
            out.insert("extra", QVariantMap{{"anilistId", id.toInt()}});
            return out;
        }
        if (url.isEmpty())
            continue;
        if (media == QLatin1String("NOVEL")) {
            if (auto source = novels::Registry::forUrl(url)) {
                out.insert("id", Library::idFor(QStringLiteral("novel"), source->key(), url));
                out.insert("media", QStringLiteral("novel"));
                out.insert("source", source->key());
                out.insert("sourceUrl", url);
                return out;
            }
        } else if (auto source = comics::Registry::forUrl(url)) {
            out.insert("id", Library::idFor(QStringLiteral("comic"), source->key(), url));
            out.insert("media", QStringLiteral("comic"));
            out.insert("source", source->key());
            out.insert("sourceUrl", url);
            return out;
        }
    }
    return {};
}

QJsonObject getJson(const QString &path) {
    const auto response = Http::get(QUrl(kSite + path), apiHeaders(), 20000);
    if (response.status == 401)
        throw Error(QStringLiteral("Your scancore.onl session ended. Sign in again."), 401);
    if (!response.ok())
        throw Error(QStringLiteral("scancore.onl responded with HTTP %1.").arg(response.status), response.status);
    return response.json().object();
}

qint64 isoSeconds(const QJsonValue &value) {
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs).toSecsSinceEpoch();
}

QString animeTitle(const QJsonObject &anime) {
    const auto title = anime.value("title").toObject();
    for (const auto *key : {"english", "romaji", "userPreferred", "native"}) {
        const auto text = title.value(QLatin1String(key)).toString();
        if (!text.isEmpty())
            return text;
    }
    return QStringLiteral("Anime");
}

// The website keeps watch progress per episode unit; this maps an AniList
// show's episode numbers to those units.
QHash<int, QString> episodeUnits(int anilistId) {
    QHash<int, QString> units;
    const auto json = getJson(QStringLiteral("/api/anime/media/%1/episodes").arg(anilistId));
    for (const auto &value : json.value("episodes").toArray()) {
        const auto episode = value.toObject();
        units.insert(episode.value("episode_number").toInt(), episode.value("unitId").toString());
    }
    return units;
}

// Local anime progress changed since the last push -> website. Mutations are
// rate limited per minute, so a long backlog goes up over several syncs.
int pushAnimeProgress(bool *more) {
    constexpr int kBatch = 30;
    const auto since = Database::pref(QStringLiteral("account.animePushedAt"), 0).toLongLong();
    const auto rows = Database::query(QStringLiteral(R"(
        SELECT series_id, unit_key, position, completed, updated_at FROM progress
        WHERE series_id LIKE 'anime:anilist:%' AND updated_at > ? ORDER BY updated_at ASC LIMIT ?
    )"),
                                      {since, kBatch});
    *more = rows.size() == kBatch;
    QHash<int, QHash<int, QString>> unitsByShow;
    qint64 pushedUpTo = since;
    int pushed = 0;
    for (const auto &value : rows) {
        const auto row = value.toMap();
        const int anilistId = row.value("series_id").toString().section(QLatin1Char(':'), 2).toInt();
        const int episode = row.value("unit_key").toInt();
        const auto position = QJsonDocument::fromJson(row.value("position").toByteArray()).object();
        double seconds = position.value("seconds").toDouble();
        double duration = position.value("duration").toDouble();
        if (duration < 1 && row.value("completed").toBool())
            seconds = duration = 1; // marked watched without playing
        try {
            QString unit;
            if (anilistId > 0 && episode > 0 && duration >= 1) {
                if (!unitsByShow.contains(anilistId)) {
                    try {
                        unitsByShow.insert(anilistId, episodeUnits(anilistId));
                    } catch (const Error &e) {
                        // A show the website does not know is skipped, not retried forever.
                        if (e.status() < 400 || e.status() >= 500 || e.status() == 401 || e.status() == 429)
                            throw;
                        unitsByShow.insert(anilistId, {});
                    }
                }
                unit = unitsByShow.value(anilistId).value(episode);
            }
            if (!unit.isEmpty()) {
                const auto body = QJsonDocument(QJsonObject{{"unitId", unit},
                                                            {"seconds", std::clamp(seconds, 0.0, 36000.0)},
                                                            {"duration", std::clamp(duration, 1.0, 36000.0)},
                                                            {"track", false}})
                                      .toJson(QJsonDocument::Compact);
                const auto response = Http::postJson(QUrl(kSite + QStringLiteral("/api/anime/progress")), body,
                                                     {{"Accept", "application/json"}});
                // Stop at the first failure that might pass later (rate
                // limit, server trouble) so nothing is skipped; an episode
                // the website rejects is.
                if (response.status == 429 || response.status >= 500 || response.status == 401 || response.status == 0)
                    break;
                if (response.ok())
                    ++pushed;
            }
        } catch (const Error &) {
            Database::setPref(QStringLiteral("account.animePushedAt"), pushedUpTo);
            throw;
        }
        pushedUpTo = row.value("updated_at").toLongLong();
    }
    Database::setPref(QStringLiteral("account.animePushedAt"), pushedUpTo);
    return pushed;
}

// Website anime progress -> local, for shows that changed there since the
// last pull. Each episode keeps whichever side saved it last.
int pullAnimeProgress() {
    const auto since = Database::pref(QStringLiteral("account.animePulledAt"), 0).toLongLong();
    qint64 newest = since;
    qint64 newestWritten = 0;
    int pulled = 0;
    for (const auto &value : getJson(QStringLiteral("/api/anime/progress")).value("progress").toArray()) {
        const auto latest = value.toObject();
        const auto updated = isoSeconds(latest.value("updatedAt"));
        const int anilistId = latest.value("anime_id").toInt();
        if (updated <= since || anilistId <= 0)
            continue;
        newest = std::max(newest, updated);
        const auto id = Library::idFor(QStringLiteral("anime"), QStringLiteral("anilist"), QString::number(anilistId));
        if (!Database::scalar(QStringLiteral("SELECT 1 FROM series WHERE id = ?"), {id}).isValid()) {
            const auto anime = latest.value("anime").toObject();
            const auto cover = anime.value("coverImage").toObject();
            Library::upsertSeries({{"id", id},
                                   {"media", "anime"},
                                   {"source", "anilist"},
                                   {"sourceUrl", QStringLiteral("https://anilist.co/anime/%1").arg(anilistId)},
                                   {"title", animeTitle(anime)},
                                   {"cover", cover.value("extraLarge").toString(cover.value("large").toString())},
                                   {"extra", QVariantMap{{"anilistId", anilistId}}}});
        }
        const auto seriesId = latest.value("seriesId").toString();
        const auto episodes =
            getJson(QStringLiteral("/api/anime/progress/series?seriesId=") + QString::fromLatin1(QUrl::toPercentEncoding(seriesId))).value("progress").toArray();
        for (const auto &item : episodes) {
            const auto episode = item.toObject();
            const int number = episode.value("episode").toInt();
            const double duration = episode.value("duration").toDouble();
            const double seconds = episode.value("seconds").toDouble();
            const bool completed = episode.value("completed").toBool();
            const auto at = isoSeconds(episode.value("updatedAt"));
            if (number <= 0)
                continue;
            const double percent = completed ? 1.0 : (duration > 0 ? std::min(1.0, seconds / duration) : 0.0);
            const auto position =
                QJsonDocument(QJsonObject{{"type", "video"}, {"seconds", seconds}, {"duration", duration}}).toJson(QJsonDocument::Compact);
            const auto before = Database::scalar(QStringLiteral("SELECT updated_at FROM progress WHERE series_id = ? AND unit_key = ?"),
                                                 {id, QString::number(number)});
            if (before.isValid() && before.toLongLong() >= at)
                continue;
            Database::exec(QStringLiteral(R"(
                INSERT INTO progress(series_id, unit_key, unit_title, unit_number, position, percent, completed, updated_at)
                VALUES(?, ?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(series_id, unit_key) DO UPDATE SET position = excluded.position, percent = excluded.percent,
                    completed = MAX(progress.completed, excluded.completed), updated_at = excluded.updated_at
            )"),
                           {id, QString::number(number), QStringLiteral("Episode %1").arg(number), QString::number(number),
                            position, percent, completed ? 1 : 0, at});
            newestWritten = std::max(newestWritten, at);
            ++pulled;
        }
    }
    Database::setPref(QStringLiteral("account.animePulledAt"), newest);
    // What just came from the website must not be pushed straight back.
    if (newestWritten > Database::pref(QStringLiteral("account.animePushedAt"), 0).toLongLong())
        Database::setPref(QStringLiteral("account.animePushedAt"), newestWritten);
    return pulled;
}

} // namespace

Account::Account(QObject *parent) : QObject(parent) {
    m_user = Database::pref(QStringLiteral("account.user")).toMap();
    if (signedIn())
        QTimer::singleShot(6000, this, [this] { sync(); });
    // Mirror saves made in the app shortly after they happen.
    QTimer *debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(4000);
    connect(debounce, &QTimer::timeout, this, [this] {
        if (signedIn() && !m_syncing)
            sync();
    });
    connect(Library::instance(), &Library::changed, this, [this, debounce] {
        if (signedIn() && !m_syncing)
            debounce->start();
    });
}

Account *Account::instance() {
    static auto *account = [] {
        auto *created = new Account;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return account;
}

QString Account::lastSync() const {
    const auto at = Database::pref(QStringLiteral("account.lastSync")).toLongLong();
    return at > 0 ? QString::number(at) : QString();
}

void Account::setUser(const QVariantMap &user) {
    m_user = user;
    Database::setPref(QStringLiteral("account.user"), user);
    emit changed();
}

Task *Account::authenticate(const QString &path, const QJsonObject &body) {
    auto *task = Task::run([path, body]() -> QVariant {
        const auto response = Http::postJson(QUrl(kSite + path), QJsonDocument(body).toJson(QJsonDocument::Compact),
                                             {{"Accept", "application/json"}});
        const auto json = QJsonDocument::fromJson(response.body).object();
        if (!response.ok()) {
            auto message = json.value("error").toString();
            if (message.isEmpty())
                message = json.value("problem").toObject().value("message").toString();
            if (response.status == 429)
                message = QStringLiteral("Too many attempts. Wait a few minutes and try again.");
            throw Error(message.isEmpty() ? QStringLiteral("scancore.onl responded with HTTP %1.").arg(response.status)
                                          : message,
                        response.status);
        }
        CookieJar::shared().save();
        return json.toVariantMap();
    });
    connect(task, &Task::finished, this, [this](const QVariant &result) {
        const auto user = result.toMap().value(QStringLiteral("user")).toMap();
        // Keep only what the app shows.
        setUser({{"username", user.value("username")}, {"email", user.value("email")}, {"avatar", user.value("avatarUrl")}});
        sync();
    });
    return task;
}

Task *Account::signIn(const QString &email, const QString &password) {
    return authenticate(QStringLiteral("/api/auth/login"), {{"email", email.trimmed()}, {"password", password}});
}

Task *Account::signUp(const QString &email, const QString &username, const QString &password) {
    return authenticate(QStringLiteral("/api/auth/signup"),
                        {{"email", email.trimmed()}, {"username", username.trimmed()}, {"password", password}});
}

QVariantList Account::passwordChecks(const QString &password) const {
    const auto check = [](const QString &label, bool passed) { return QVariantMap{{"label", label}, {"passed", passed}}; };
    return {check(QStringLiteral("At least 8 characters"), password.size() >= 8 && password.size() <= 128),
            check(QStringLiteral("A lowercase letter"), password.contains(QRegularExpression(QStringLiteral("[a-z]")))),
            check(QStringLiteral("An uppercase letter"), password.contains(QRegularExpression(QStringLiteral("[A-Z]")))),
            check(QStringLiteral("A number"), password.contains(QRegularExpression(QStringLiteral("\\d")))),
            check(QStringLiteral("A symbol"), password.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9]"))))};
}

QString Account::usernameProblem(const QString &username) const {
    const auto name = username.trimmed();
    if (name.isEmpty())
        return {};
    if (name.size() < 3)
        return QStringLiteral("At least 3 characters");
    if (name.size() > 32)
        return QStringLiteral("At most 32 characters");
    if (!QRegularExpression(QStringLiteral("^[a-zA-Z0-9_]+$")).match(name).hasMatch())
        return QStringLiteral("Letters, numbers and underscores only");
    return {};
}

void Account::signOut() {
    runInBackground([] {
        try {
            Http::postJson(QUrl(kSite + QStringLiteral("/api/auth/logout")), "{}", {{"Accept", "application/json"}});
        } catch (const Error &) {
        }
        for (const auto &cookie : CookieJar::shared().cookiesForUrl(QUrl(kSite)))
            CookieJar::shared().deleteCookie(cookie);
        CookieJar::shared().save();
    });
    setUser({});
    Database::setPref(QStringLiteral("account.lastSync"), 0);
    // A different account starts from scratch, uploading this device's history.
    Database::setPref(QStringLiteral("account.animePushedAt"), 0);
    Database::setPref(QStringLiteral("account.animePulledAt"), 0);
}

QVariantMap Account::importAll(const QJsonObject &session) {
    int libraryImported = 0;
    int historyImported = 0;
    int pushed = 0;
    QSet<QString> remoteIds;

    // Website library -> local library.
    QString cursor;
    for (int page = 0; page < 20; ++page) {
        const auto json =
            getJson(QStringLiteral("/api/library?limit=500") + (cursor.isEmpty() ? QString() : QStringLiteral("&cursor=") + cursor));
        for (const auto &value : json.value("entries").toArray()) {
            const auto entry = value.toObject();
            auto series = localSeries(entry.value("series").toObject());
            if (series.isEmpty())
                continue;
            const auto id = series.value("id").toString();
            remoteIds.insert(id);
            if (!Library::instance()->isSaved(id)) {
                Library::upsertSeries(series);
                Database::exec(QStringLiteral("INSERT OR IGNORE INTO library(series_id, category, added_at) VALUES(?, ?, ?)"),
                               {id, categoryFor(entry.value("status").toString()), Database::now()});
                ++libraryImported;
            }
        }
        cursor = json.value("nextCursor").toString();
        if (cursor.isEmpty())
            break;
    }

    // Website comic reading history -> local progress, when newer.
    for (const auto &value : session.value("history").toArray()) {
        const auto entry = value.toObject();
        const auto comic = entry.value("comic").toObject();
        const auto chapter = entry.value("chapter").toObject();
        const auto url = comic.value("url").toString();
        const auto chapterUrl = chapter.value("episode_url").toString();
        auto source = comics::Registry::forUrl(url);
        if (!source || chapterUrl.isEmpty())
            continue;
        const auto id = Library::idFor(QStringLiteral("comic"), source->key(), url);
        const auto updated = qint64(entry.value("updatedAt").toDouble() / 1000);
        const auto local = Database::scalar(QStringLiteral("SELECT MAX(updated_at) FROM progress WHERE series_id = ?"), {id});
        if (local.isValid() && local.toLongLong() >= updated)
            continue;
        Library::upsertSeries({{"id", id},
                               {"media", "comic"},
                               {"source", source->key()},
                               {"sourceUrl", url},
                               {"title", comic.value("title").toString()},
                               {"cover", comic.value("thumbnail").toString()}});
        const int total = std::max(1, entry.value("totalPanels").toInt(1));
        const int panel = entry.value("panelIndex").toInt();
        const double percent = std::min(1.0, double(panel + 1) / total);
        const auto position = QJsonDocument(QJsonObject{{"type", "strip"}, {"panel", panel}}).toJson(QJsonDocument::Compact);
        Database::exec(QStringLiteral(R"(
            INSERT INTO progress(series_id, unit_key, unit_title, unit_number, unit_url, position, percent, completed, updated_at)
            VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(series_id, unit_key) DO UPDATE SET position = excluded.position, percent = excluded.percent,
                completed = MAX(progress.completed, excluded.completed), updated_at = excluded.updated_at
        )"),
                       {id, chapterUrl, chapter.value("title").toString(), chapter.value("episode_no").toVariant().toString(),
                        chapterUrl, position, percent, percent >= 0.98 ? 1 : 0, updated > 0 ? updated : Database::now()});
        ++historyImported;
    }

    // Local library -> website, for series the website already knows.
    QStringList comicUrls;
    const auto local = Database::query(QStringLiteral(
        "SELECT s.id, s.media, s.source_url, l.category FROM library l JOIN series s ON s.id = l.series_id"));
    for (const auto &value : local) {
        const auto row = value.toMap();
        const auto id = row.value("id").toString();
        if (remoteIds.contains(id))
            continue;
        const auto url = row.value("source_url").toString();
        if (row.value("media").toString() == QLatin1String("comic")) {
            comicUrls.append(url);
            continue;
        }
        const auto body = QJsonDocument(QJsonObject{{"sourceUrl", url}, {"status", statusFor(row.value("category").toString())}})
                              .toJson(QJsonDocument::Compact);
        try {
            if (Http::postJson(QUrl(kSite + QStringLiteral("/api/library")), body, {{"Accept", "application/json"}}).ok())
                ++pushed;
        } catch (const Error &) {
        }
    }
    for (int i = 0; i < comicUrls.size(); i += 120) {
        QJsonArray urls;
        for (const auto &url : comicUrls.mid(i, 120))
            urls.append(url);
        try {
            const auto response = Http::postJson(QUrl(kSite + QStringLiteral("/api/library/import")),
                                                 QJsonDocument(QJsonObject{{"urls", urls}}).toJson(QJsonDocument::Compact),
                                                 {{"Accept", "application/json"}});
            if (response.ok())
                pushed += QJsonDocument::fromJson(response.body).object().value("imported").toInt();
        } catch (const Error &) {
        }
    }
    // Anime watch progress, both ways: local changes first, so a pull cannot
    // overwrite something newer that has not gone up yet.
    int animePushed = 0;
    int animePulled = 0;
    bool morePending = false;
    try {
        animePushed = pushAnimeProgress(&morePending);
        animePulled = pullAnimeProgress();
    } catch (const Error &e) {
        if (e.status() == 401)
            throw;
        qWarning().noquote() << "Anime progress sync:" << e.message();
    }
    return {{"libraryImported", libraryImported},
            {"historyImported", historyImported + animePulled},
            {"pushed", pushed + animePushed},
            {"morePending", morePending}};
}

Task *Account::sync() {
    m_syncing = true;
    emit changed();
    auto *task = Task::run([]() -> QVariant {
        const auto session = getJson(QStringLiteral("/api/auth/session"));
        if (session.value("user").isNull())
            throw Error(QStringLiteral("Your scancore.onl session ended. Sign in again."), 401);
        return importAll(session);
    });
    connect(task, &Task::changed, this, [this, task] {
        if (task->loading())
            return;
        // Notify while still marked as syncing so the library-change hook
        // does not start another sync.
        if (task->ok()) {
            Database::setPref(QStringLiteral("account.lastSync"), Database::now());
            Library::instance()->notifyChanged();
            // The website limits writes per minute; send the rest shortly.
            if (task->result().toMap().value(QStringLiteral("morePending")).toBool())
                QTimer::singleShot(65000, this, [this] {
                    if (signedIn() && !m_syncing)
                        sync();
                });
        } else if (task->status() == 401) {
            setUser({});
        }
        m_syncing = false;
        emit changed();
    });
    return task;
}

} // namespace sc
