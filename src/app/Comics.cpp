#include "Comics.h"

#include "App.h"
#include "Downloads.h"
#include "Library.h"
#include "Resolve.h"
#include "core/Database.h"
#include "core/ImageCache.h"
#include "core/Parallel.h"
#include "core/Text.h"
#include "extensions/ExtensionSource.h"
#include "sources/Manifest.h"
#include "sources/catalogue/Catalogue.h"
#include "sources/comics/Registry.h"

#include <QCache>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QUrlQuery>

namespace sc {

using namespace sc::comics;

namespace {

QMutex g_searchMutex;
QCache<QString, QVariantMap> g_searches(40);
QHash<QString, qint64> g_searchTimes;

std::shared_ptr<Source> requireSource(const QString &url) {
    auto source = Registry::forUrl(url);
    if (!source)
        throw Error(QStringLiteral("SCANCORE does not support %1 yet.").arg(QUrl(url).host()));
    return source;
}

// Exact title, then prefix, then all words, then the rest, preserving each
// source's own order within a tier.
int relevance(const QString &title, const QString &query) {
    const auto t = normalizedTitle(title);
    const auto q = normalizedTitle(query);
    if (t == q)
        return 0;
    if (t.startsWith(q))
        return 1;
    const auto words = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (std::all_of(words.begin(), words.end(), [&](const QString &w) { return t.contains(w); }))
        return 2;
    return 3;
}

// Settings → Sources can limit "All sources" to one content language;
// empty (the default) searches every language. Files on this device are
// always searched.
QString searchLanguage() { return Database::pref(QStringLiteral("setting.sourceLanguages")).toString(); }

bool inSearchLanguage(const Source &source, const QString &language) {
    return language.isEmpty() || source.key() == QLatin1String("local") ||
           source.language().section(QLatin1Char('-'), 0, 0) == language;
}

QVariantList chaptersToUnits(const QList<Chapter> &chapters) {
    QVariantList units;
    for (const auto &c : chapters) {
        auto map = c.toMap();
        map.insert(QStringLiteral("key"), c.url);
        units.append(map);
    }
    return units;
}

} // namespace

QVariantList Comics::sources() const {
    QVariantList out;
    for (const auto &source : Registry::all())
        out.append(QVariantMap{{"key", source->key()},
                               {"label", source->label()},
                               {"enabled", source->enabled()},
                               {"disabledReason", source->config().disabledReason},
                               {"nsfw", source->config().nsfw},
                               {"language", source->language()},
                               {"languageName", languageName(source->language())},
                               {"base", source->config().base},
                               {"hasLatest", source->hasLatest()}});
    return out;
}

QVariantList Comics::languages() const {
    QVariantList out;
    QSet<QString> seen;
    for (const auto &source : Registry::all())
        if (source->enabled() && source->key() != QLatin1String("local") && !seen.contains(source->language())) {
            seen.insert(source->language());
            out.append(QVariantMap{{"value", source->language()}, {"label", languageName(source->language())}});
        }
    return out;
}

QString Comics::sourceLabel(const QString &key) const {
    auto source = Registry::byKey(key);
    return source ? source->label() : key;
}

QString Comics::sourceForUrl(const QString &url) const {
    auto source = Registry::forUrl(url);
    return source ? source->key() : QString();
}

QString Comics::seriesId(const QString &url) const {
    return Library::idFor(QStringLiteral("comic"), sourceForUrl(url), url);
}

void Comics::shareSiteCookies(const QString &url) {
    runInBackground([url] {
        try {
            ext::ExtensionSource::shareCookies(url);
        } catch (const Error &) {
            // The extension asks again if it still needs the check.
        }
    });
}

QString Comics::imageReferer(const QString &url) const {
    auto source = Registry::forUrl(url);
    return source ? source->imageReferer() : QString();
}

void Comics::prefetch(const QVariantList &panels, const QString &referer, int count) {
    QStringList urls;
    for (const auto &value : panels.mid(0, count))
        urls.append(value.toMap().value(QStringLiteral("url")).toString());
    runInBackground([urls, referer] {
        for (const auto &url : urls)
            if (!url.startsWith(QLatin1String("file:")))
                try {
                    ImageCache::fetch(url, referer);
                } catch (const Error &) {
                }
    });
}

Task *Comics::search(const QString &query, const QString &only) {
    const auto q = query.simplified();
    const auto language = only == QLatin1String("all") ? searchLanguage() : QString();
    const auto cacheKey = q.toLower() + QLatin1Char('|') + only + QLatin1Char('|') + language + QLatin1Char('|') + contentLanguage();
    {
        QMutexLocker lock(&g_searchMutex);
        if (auto *hit = g_searches.object(cacheKey);
            hit && QDateTime::currentSecsSinceEpoch() - g_searchTimes.value(cacheKey) < 180)
            return Task::resolved(*hit);
    }
    return Task::run([q, only, language, cacheKey]() -> QVariant {
        // Files on this device are listed without a search.
        if (q.isEmpty() && only != QLatin1String("local"))
            return QVariantMap{{"comics", QVariantList()}, {"providers", QVariantList()}};
        QList<std::shared_ptr<Source>> selected;
        for (const auto &source : Registry::all())
            if (source->enabled() && (only == QLatin1String("all") ? inSearchLanguage(*source, language) : source->key() == only))
                selected.append(source);
        if (selected.isEmpty())
            throw Error(QStringLiteral("That source is unavailable."));

        QList<std::function<QList<Summary>()>> jobs;
        for (const auto &source : selected)
            jobs.append([source, q] { return source->search(q); });
        const auto results = parallel(jobs);

        struct Ranked {
            int tier;
            bool preferred;
            int order;
            Summary item;
        };
        const auto wantedLanguage = contentLanguage();
        QList<Ranked> ranked;
        QVariantList providers;
        QSet<QString> seen;
        bool anyOk = false;
        for (int i = 0; i < results.size(); ++i) {
            const auto &r = results[i];
            providers.append(QVariantMap{{"source", selected[i]->key()},
                                         {"label", selected[i]->label()},
                                         {"ok", r.ok()},
                                         {"count", r.ok() ? r.value->size() : 0},
                                         {"error", r.error},
                                         {"verifyUrl", r.verifyUrl}});
            if (!r.ok())
                continue;
            anyOk = true;
            int order = 0;
            for (const auto &item : *r.value) {
                if (item.title.isEmpty() || item.url.isEmpty() || seen.contains(item.url))
                    continue;
                seen.insert(item.url);
                ranked.append({relevance(item.title, q), baseLanguage(selected[i]->language()) == wantedLanguage, order++, item});
            }
        }
        if (!anyOk)
            throw Error(QStringLiteral("Comic sources are unavailable right now. Check your connection and try again."));
        // Equally good titles in the content language first.
        std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
            if (a.tier != b.tier)
                return a.tier < b.tier;
            if (a.preferred != b.preferred)
                return a.preferred;
            return a.order < b.order;
        });
        QVariantList comics;
        for (const auto &r : ranked) {
            auto map = r.item.toMap();
            map.insert(QStringLiteral("id"), Library::idFor(QStringLiteral("comic"), r.item.source, r.item.url));
            map.insert(QStringLiteral("media"), QStringLiteral("comic"));
            map.insert(QStringLiteral("sourceUrl"), r.item.url);
            map.insert(QStringLiteral("sourceLabel"), Registry::byKey(r.item.source)
                                                          ? Registry::byKey(r.item.source)->label()
                                                          : r.item.source);
            comics.append(map);
        }
        QVariantMap result{{"comics", comics}, {"providers", providers}};
        QMutexLocker lock(&g_searchMutex);
        g_searches.insert(cacheKey, new QVariantMap(result));
        g_searchTimes.insert(cacheKey, QDateTime::currentSecsSinceEpoch());
        return result;
    });
}

namespace {

QVariantList entryMaps(const QList<catalogue::Entry> &entries) {
    QVariantList out;
    for (const auto &e : entries)
        out.append(e.toMap());
    return out;
}

} // namespace

QVariantMap Comics::catalogShelves(const QString &media, bool mature) {
    auto catalogue = catalogue::forMedia(catalogue::chosen(media), media);
    if (!catalogue)
        return QVariantMap{{"catalog", QString()}, {"shelves", QVariantList()}};
    const auto cacheKey = QStringLiteral("catalogue:%1:%2:%3").arg(catalogue->key(), media, mature ? "all" : "sfw");
    if (auto cached = Database::cacheGet(cacheKey))
        return QJsonDocument::fromJson(*cached).toVariant().toMap();
    QList<catalogue::Shelf> shelves;
    try {
        shelves = catalogue->shelves(media, mature);
    } catch (const Error &) {
        if (auto stale = Database::cacheGet(cacheKey, true))
            return QJsonDocument::fromJson(*stale).toVariant().toMap();
        throw;
    }
    QVariantList list;
    for (const auto &shelf : shelves)
        if (!shelf.entries.isEmpty())
            list.append(QVariantMap{{"key", shelf.key}, {"items", entryMaps(shelf.entries)}});
    const QVariantMap result{{"catalog", catalogue->key()}, {"label", catalogue->label()}, {"shelves", list}};
    Database::cachePut(cacheKey, QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact), 1800);
    return result;
}

QVariantMap Comics::catalogSearch(const QString &media, const QString &query, bool mature) {
    auto catalogue = catalogue::forMedia(catalogue::chosen(media), media);
    if (!catalogue)
        throw Error(QStringLiteral("No catalogue is chosen. Pick one in Settings → Connections."));
    const auto cacheKey = QStringLiteral("catalogue|%1|%2|%3|%4").arg(catalogue->key(), media, query.toLower()).arg(mature);
    {
        QMutexLocker lock(&g_searchMutex);
        if (auto *hit = g_searches.object(cacheKey);
            hit && QDateTime::currentSecsSinceEpoch() - g_searchTimes.value(cacheKey) < 180)
            return *hit;
    }
    const auto entries = catalogue->search(media, query, mature);
    // Comics and novels pages read the list under their own name.
    QVariantMap result{{media == QLatin1String("novel") ? "books" : "comics", entryMaps(entries)},
                       {"providers", QVariantList()},
                       {"catalog", catalogue->key()},
                       {"label", catalogue->label()}};
    QMutexLocker lock(&g_searchMutex);
    g_searches.insert(cacheKey, new QVariantMap(result));
    g_searchTimes.insert(cacheKey, QDateTime::currentSecsSinceEpoch());
    return result;
}

QStringList Comics::catalogNames(const QVariantMap &item) {
    const auto entry = catalogue::Entry::fromMap(item);
    QStringList names{entry.title};
    if (auto catalogue = catalogue::byKey(entry.catalog))
        names += catalogue->names(entry);
    else
        names += entry.titles;
    return names;
}

Task *Comics::catalog(const QString &query) {
    const auto q = query.simplified();
    const bool mature = App::instance()->settings().value(QStringLiteral("showNsfwSources")).toBool();
    return Task::run([q, mature]() -> QVariant { return catalogSearch(QStringLiteral("comic"), q, mature); });
}

Task *Comics::catalogFeeds(bool mature) {
    return Task::run([mature]() -> QVariant { return catalogShelves(QStringLiteral("comic"), mature); });
}

Task *Comics::resolve(const QVariantMap &item) {
    return Task::run([item]() -> QVariant {
        const auto id = item.value("catalogId").toString().isEmpty() ? item.value("hid").toString()
                                                                       : item.value("catalogId").toString();
        const auto language = searchLanguage();
        // Matches depend on the source list and the language settings.
        const auto cacheKey = QStringLiteral("resolve:comic:%1:%2:%3:%4:%5")
                                  .arg(Manifest::version())
                                  .arg(language, contentLanguage(), item.value("catalog").toString(), id);
        if (!id.isEmpty())
            if (auto cached = Database::cacheGet(cacheKey))
                return QJsonDocument::fromJson(*cached).toVariant();
        const auto names = catalogNames(item);
        if (std::all_of(names.begin(), names.end(), [](const QString &n) { return normalizedTitle(n).isEmpty(); }))
            throw Error(QStringLiteral("This title has no name SCANCORE's sources could list it under."));
        QList<std::shared_ptr<Source>> sources;
        for (const auto &source : Registry::all())
            if (source->enabled() && source->key() != QLatin1String("local") && inSearchLanguage(*source, language))
                sources.append(source);
        const auto matches = resolveAcross<std::shared_ptr<Source>>(
            sources, names, [](const std::shared_ptr<Source> &source, const QString &url) { return source->readable(url); });
        const QVariantMap result{{"matches", matches}, {"contentLanguage", contentLanguage()}};
        if (!id.isEmpty() && !matches.isEmpty())
            Database::cachePut(cacheKey, QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact), 86400);
        return result;
    });
}

QVariantList Comics::fetchUnits(const QString &url) { return chaptersToUnits(requireSource(url)->chapters(url)); }

Task *Comics::series(const QString &url, const QVariantMap &summary) {
    return Task::run([url, summary]() -> QVariant {
        auto source = requireSource(url);
        const auto id = Library::idFor(QStringLiteral("comic"), source->key(), url);

        QVariantMap series = summary;
        series.insert(QStringLiteral("id"), id);
        series.insert(QStringLiteral("media"), QStringLiteral("comic"));
        series.insert(QStringLiteral("source"), source->key());
        series.insert(QStringLiteral("sourceLabel"), source->label());
        series.insert(QStringLiteral("sourceUrl"), url);
        // Fill gaps from what is already stored (library or history).
        const auto stored = Library::instance()->seriesRow(id);
        for (auto it = stored.begin(); it != stored.end(); ++it)
            if (series.value(it.key()).toString().isEmpty() && !it.value().toString().isEmpty())
                series.insert(it.key(), it.value());

        const auto detailsKey = QStringLiteral("comic-details:") + url;
        std::optional<Details> cachedDetails;
        if (auto cached = Database::cacheGet(detailsKey)) {
            const auto o = QJsonDocument::fromJson(*cached).object();
            cachedDetails = Details{o.value("title").toString(),  o.value("cover").toString(),
                                    o.value("description").toString(), o.value("author").toString(),
                                    o.value("status").toString(), o.value("genres").toVariant().toStringList(),
                                    o.value("trackerIds").toObject().toVariantMap()};
        }

        QList<std::function<QVariant()>> jobs;
        jobs.append([source, url]() -> QVariant { return chaptersToUnits(source->chapters(url)); });
        if (!cachedDetails)
            jobs.append([source, url]() -> QVariant {
                const auto d = source->details(url);
                if (!d)
                    return {};
                return QVariantMap{{"title", d->title},   {"cover", d->cover},   {"description", d->description},
                                   {"author", d->author}, {"status", d->status}, {"genres", d->genres},
                                   {"trackerIds", d->trackerIds}};
            });
        const auto results = parallel(jobs);

        QVariantMap details;
        if (cachedDetails)
            details = QVariantMap{{"title", cachedDetails->title},       {"cover", cachedDetails->cover},
                                  {"description", cachedDetails->description}, {"author", cachedDetails->author},
                                  {"status", cachedDetails->status},     {"genres", cachedDetails->genres},
                                  {"trackerIds", cachedDetails->trackerIds}};
        else if (results.size() > 1 && results[1].ok() && results[1].value->isValid()) {
            details = results[1].value->toMap();
            Database::cachePut(detailsKey, QJsonDocument::fromVariant(details).toJson(QJsonDocument::Compact), 86400);
        }
        // AniList and MyAnimeList ids the source knows go with the series
        // (series.extra), next to a Comick catalogue entry if it has one;
        // tracking uses them before searching by title.
        if (const auto trackerIds = details.take(QStringLiteral("trackerIds")).toMap(); !trackerIds.isEmpty()) {
            auto extra = series.value(QStringLiteral("extra")).toMap();
            if (extra.isEmpty())
                extra = stored.value(QStringLiteral("extra")).toMap();
            for (auto it = trackerIds.begin(); it != trackerIds.end(); ++it)
                extra.insert(it.key(), it.value());
            series.insert(QStringLiteral("extra"), extra);
        }
        for (auto it = details.begin(); it != details.end(); ++it) {
            const bool empty = it.value().typeId() == QMetaType::QStringList ? it.value().toStringList().isEmpty()
                                                                               : it.value().toString().isEmpty();
            // A source's own cover/title wins over a search thumbnail only
            // when the summary had none.
            if (!empty && (series.value(it.key()).toString().isEmpty() || it.key() == QLatin1String("description") ||
                           it.key() == QLatin1String("genres") || it.key() == QLatin1String("status")))
                series.insert(it.key(), it.value());
        }

        QVariantMap result{{"series", series}};
        if (results[0].ok()) {
            const auto units = results[0].value->toList();
            Library::upsertSeries(series);
            Library::storeUnits(id, units);
            result.insert(QStringLiteral("chapters"), units);
            result.insert(QStringLiteral("offline"), false);
        } else {
            const auto cached = Library::cachedUnits(id);
            if (cached.isEmpty())
                throw Error(results[0].error, results[0].status, results[0].verifyUrl);
            result.insert(QStringLiteral("chapters"), cached);
            result.insert(QStringLiteral("offline"), true);
            result.insert(QStringLiteral("error"), results[0].error);
        }
        return result;
    });
}

Task *Comics::panels(const QString &chapterUrl, const QString &seriesId, const QString &unitKey) {
    return Task::run([chapterUrl, seriesId, unitKey]() -> QVariant {
        // Downloaded chapters are read from disk, online or not.
        if (!seriesId.isEmpty()) {
            const auto local = Downloads::localPanels(seriesId, unitKey.isEmpty() ? chapterUrl : unitKey);
            if (!local.isEmpty())
                return QVariantMap{{"panels", local}, {"referer", QString()}, {"offline", true}};
        }
        auto source = requireSource(chapterUrl);
        const auto panels = source->panels(chapterUrl);
        if (panels.isEmpty())
            throw Error(QStringLiteral("This chapter has no readable pages on %1. It may be locked or removed.")
                            .arg(source->label()));
        QVariantList list;
        for (const auto &p : panels)
            list.append(QVariantMap{{"url", p.url}, {"width", p.width}, {"height", p.height}});
        return QVariantMap{{"panels", list}, {"referer", source->imageReferer()}, {"offline", false}};
    });
}

Task *Comics::latest(int limit) {
    return Task::run([limit]() -> QVariant {
        const auto cacheKey = QStringLiteral("comic-latest:%1").arg(limit);
        if (auto cached = Database::cacheGet(cacheKey))
            return QJsonDocument::fromJson(*cached).toVariant();
        QList<std::shared_ptr<Source>> selected;
        for (const auto &source : Registry::all())
            if (source->enabled() && source->hasLatest())
                selected.append(source);
        QList<std::function<QList<LatestItem>()>> jobs;
        for (const auto &source : selected)
            jobs.append([source, limit] { return source->latest(limit); });
        const auto results = parallel(jobs);
        QList<LatestItem> all;
        QVariantList providers;
        for (int i = 0; i < results.size(); ++i) {
            providers.append(QVariantMap{{"source", selected[i]->key()}, {"ok", results[i].ok()}, {"error", results[i].error}});
            if (results[i].ok())
                all.append(*results[i].value);
        }
        if (all.isEmpty() && !results.isEmpty()) {
            if (auto stale = Database::cacheGet(cacheKey, true))
                return QJsonDocument::fromJson(*stale).toVariant();
            throw Error(QStringLiteral("Latest updates are unavailable right now."));
        }
        std::stable_sort(all.begin(), all.end(), [](const LatestItem &a, const LatestItem &b) {
            return QDateTime::fromString(a.publishedAt, Qt::ISODate) > QDateTime::fromString(b.publishedAt, Qt::ISODate);
        });
        QVariantList updates;
        QSet<QString> seen;
        for (const auto &item : all) {
            if (seen.contains(item.url))
                continue;
            seen.insert(item.url);
            auto map = item.toMap();
            map.insert(QStringLiteral("id"), Library::idFor(QStringLiteral("comic"), item.source, item.url));
            map.insert(QStringLiteral("media"), QStringLiteral("comic"));
            map.insert(QStringLiteral("sourceUrl"), item.url);
            updates.append(map);
            if (updates.size() >= limit)
                break;
        }
        QVariantMap result{{"updates", updates}, {"providers", providers}};
        Database::cachePut(cacheKey, QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact), 300);
        return result;
    });
}

} // namespace sc
