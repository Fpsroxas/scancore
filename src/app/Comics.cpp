#include "Comics.h"

#include "Downloads.h"
#include "Library.h"
#include "core/Database.h"
#include "core/ImageCache.h"
#include "core/Parallel.h"
#include "sources/comics/Registry.h"

#include <QCache>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QRegularExpression>
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

QString normalized(const QString &text) {
    static const QRegularExpression nonWord(QStringLiteral("[^a-z0-9]+"));
    return text.toLower().replace(nonWord, QStringLiteral(" ")).simplified();
}

// Exact title, then prefix, then all words, then the rest, preserving each
// source's own order within a tier.
int relevance(const QString &title, const QString &query) {
    const auto t = normalized(title);
    const auto q = normalized(query);
    if (t == q)
        return 0;
    if (t.startsWith(q))
        return 1;
    const auto words = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (std::all_of(words.begin(), words.end(), [&](const QString &w) { return t.contains(w); }))
        return 2;
    return 3;
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
                               {"base", source->config().base},
                               {"hasLatest", source->hasLatest()}});
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
    const auto cacheKey = q.toLower() + QLatin1Char('|') + only;
    {
        QMutexLocker lock(&g_searchMutex);
        if (auto *hit = g_searches.object(cacheKey);
            hit && QDateTime::currentSecsSinceEpoch() - g_searchTimes.value(cacheKey) < 180)
            return Task::resolved(*hit);
    }
    return Task::run([q, only, cacheKey]() -> QVariant {
        // Files on this device are listed without a search.
        if (q.isEmpty() && only != QLatin1String("local"))
            return QVariantMap{{"comics", QVariantList()}, {"providers", QVariantList()}};
        QList<std::shared_ptr<Source>> selected;
        for (const auto &source : Registry::all())
            if (source->enabled() && (only == QLatin1String("all") || source->key() == only))
                selected.append(source);
        if (selected.isEmpty())
            throw Error(QStringLiteral("That source is unavailable."));

        QList<std::function<QList<Summary>()>> jobs;
        for (const auto &source : selected)
            jobs.append([source, q] { return source->search(q); });
        const auto results = parallel(jobs);

        struct Ranked {
            int tier;
            int order;
            Summary item;
        };
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
                ranked.append({relevance(item.title, q), order++, item});
            }
        }
        if (!anyOk)
            throw Error(QStringLiteral("Comic sources are unavailable right now. Check your connection and try again."));
        std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
            return a.tier != b.tier ? a.tier < b.tier : a.order < b.order;
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

// One entry of Comick's catalogue, as the comic pages and "Read from" use it.
QVariantMap catalogItem(const QJsonObject &c) {
    static const QHash<QString, QString> kinds{{"kr", "Manhwa"}, {"cn", "Manhua"}, {"jp", "Manga"}};
    static const QStringList statuses{QString(), QStringLiteral("Ongoing"), QStringLiteral("Completed"), QStringLiteral("Cancelled"),
                                      QStringLiteral("Hiatus")};
    QStringList titles{c.value("title").toString()};
    for (const auto &t : c.value("md_titles").toArray())
        titles.append(t.toObject().value("title").toString());
    titles.removeAll(QString());
    titles.removeDuplicates();
    const auto covers = c.value("md_covers").toArray();
    auto key = covers.isEmpty() ? QString() : covers.first().toObject().value("b2key").toString();
    // The medium size ("-m", always JPEG); the original can be half a megabyte.
    if (const auto dot = key.lastIndexOf(QLatin1Char('.')); dot > 0)
        key = key.left(dot) + QStringLiteral("-m.jpg");
    const auto lastChapter = c.value("last_chapter");
    return QVariantMap{{"catalog", QStringLiteral("comick")},
                       {"hid", c.value("hid").toString()},
                       {"slug", c.value("slug").toString()},
                       {"media", QStringLiteral("comic")},
                       {"title", titles.value(0)},
                       {"titles", titles},
                       {"cover", key.isEmpty() ? QString() : QStringLiteral("https://meo.comick.pictures/") + key},
                       {"type", kinds.value(c.value("country").toString())},
                       {"status", statuses.value(c.value("status").toInt())},
                       {"year", c.value("year").toInt()},
                       {"lastChapter", lastChapter.isDouble() ? QString::number(lastChapter.toDouble()) : QString()},
                       {"rating", c.value("bayesian_rating").toString()},
                       {"description", c.value("desc").toString()},
                       {"contentRating", c.value("content_rating").toString()},
                       {"genres", QVariantList()}};
}

const Headers kComickHeaders{{"Accept", "application/json"}, {"Referer", "https://comick.dev/"}};

} // namespace

Task *Comics::catalog(const QString &query) {
    const auto q = query.simplified();
    const auto cacheKey = QStringLiteral("comick|") + q.toLower();
    {
        QMutexLocker lock(&g_searchMutex);
        if (auto *hit = g_searches.object(cacheKey);
            hit && QDateTime::currentSecsSinceEpoch() - g_searchTimes.value(cacheKey) < 180)
            return Task::resolved(*hit);
    }
    return Task::run([q, cacheKey]() -> QVariant {
        QUrl url(QStringLiteral("https://api.comick.dev/v1.0/search/"));
        url.setQuery(QUrlQuery{{"q", q.left(100)}, {"page", "1"}, {"limit", "40"}, {"showall", "false"}});
        const auto response = Http::get(url, kComickHeaders);
        if (!response.ok())
            throw Error(QStringLiteral("Comick's catalogue is unavailable right now (%1).").arg(response.status), response.status);
        QVariantList comics;
        for (const auto &value : response.json().array()) {
            const auto c = value.toObject();
            if (c.value("media_type").toString() != QLatin1String("anime"))
                comics.append(catalogItem(c));
        }
        QVariantMap result{{"comics", comics}, {"providers", QVariantList()}};
        QMutexLocker lock(&g_searchMutex);
        g_searches.insert(cacheKey, new QVariantMap(result));
        g_searchTimes.insert(cacheKey, QDateTime::currentSecsSinceEpoch());
        return result;
    });
}

Task *Comics::catalogFeeds(bool mature) {
    return Task::run([mature]() -> QVariant {
        const auto cacheKey = QStringLiteral("comick-top:") + (mature ? "all" : "sfw");
        if (auto cached = Database::cacheGet(cacheKey))
            return QJsonDocument::fromJson(*cached).toVariant();
        const auto response = Http::get(QUrl(QStringLiteral("https://api.comick.dev/top?accept_mature_content=%1")
                                                 .arg(mature ? "true" : "false")),
                                        kComickHeaders);
        if (!response.ok())
            throw Error(QStringLiteral("Comick's catalogue is unavailable right now (%1).").arg(response.status), response.status);
        const auto top = response.json().object();
        const auto list = [mature](const QJsonValue &value) {
            // Some feeds are by period ("7", "30", "90" days): the week's.
            auto array = value.toArray();
            if (value.isObject()) {
                const auto byDays = value.toObject();
                array = (byDays.contains("7") ? byDays.value("7") : byDays.value("30")).toArray();
            }
            QVariantList out;
            for (const auto &item : array) {
                const auto c = item.toObject();
                if (!mature && c.value("content_rating").toString() == QLatin1String("erotica"))
                    continue;
                out.append(catalogItem(c));
                if (out.size() >= 24)
                    break;
            }
            return out;
        };
        const QVariantMap feeds{{"trending", list(top.value("trending"))},
                                {"popular", list(top.value("rank"))},
                                {"newest", list(top.value("news"))},
                                {"completed", list(top.value("completions"))}};
        Database::cachePut(cacheKey, QJsonDocument::fromVariant(feeds).toJson(QJsonDocument::Compact), 1800);
        return feeds;
    });
}

Task *Comics::resolve(const QVariantMap &item) {
    return Task::run([item]() -> QVariant {
        const auto hid = item.value("hid").toString();
        const auto cacheKey = QStringLiteral("comick-resolve:") + hid;
        if (!hid.isEmpty())
            if (auto cached = Database::cacheGet(cacheKey))
                return QJsonDocument::fromJson(*cached).toVariant();
        // Feed entries carry one title; the catalogue knows every name.
        auto known = item.value("titles").toStringList();
        if (known.size() < 2 && !hid.isEmpty()) {
            const auto details = Http::get(QUrl(QStringLiteral("https://api.comick.dev/v1.0/comic/%1/").arg(hid)), kComickHeaders);
            if (details.ok()) {
                const auto comic = details.json().object().value("comic").toObject();
                known.append(comic.value("title").toString());
                for (const auto &t : comic.value("md_titles").toArray())
                    known.append(t.toObject().value("title").toString());
            }
        }
        // Every name it goes by, as sources write titles in Latin letters.
        QStringList names;
        QSet<QString> wanted;
        for (const auto &title : QStringList{item.value("title").toString()} + known) {
            const auto n = normalized(title);
            if (n.isEmpty() || wanted.contains(n))
                continue;
            wanted.insert(n);
            names.append(title);
        }
        if (names.isEmpty())
            throw Error(QStringLiteral("This title has no name SCANCORE's sources could list it under."));
        QList<std::shared_ptr<Source>> sources;
        for (const auto &source : Registry::all())
            if (source->enabled() && source->key() != QLatin1String("local"))
                sources.append(source);
        // The main title first; other names only for sources that missed.
        QVariantList matches;
        QSet<QString> found;
        for (int round = 0; round < std::min<int>(names.size(), 3) && found.size() < sources.size(); ++round) {
            QList<std::shared_ptr<Source>> asked;
            QList<std::function<QList<Summary>()>> jobs;
            for (const auto &source : sources)
                if (!found.contains(source->key())) {
                    asked.append(source);
                    jobs.append([source, name = names[round]] { return source->search(name); });
                }
            const auto results = parallel(jobs);
            for (int i = 0; i < results.size(); ++i) {
                if (!results[i].ok())
                    continue;
                for (const auto &summary : *results[i].value)
                    if (wanted.contains(normalized(summary.title)) && !found.contains(asked[i]->key())) {
                        found.insert(asked[i]->key());
                        matches.append(QVariantMap{{"source", asked[i]->key()},
                                                   {"sourceLabel", asked[i]->label()},
                                                   {"url", summary.url},
                                                   {"title", summary.title},
                                                   {"cover", summary.cover},
                                                   {"order", sources.indexOf(asked[i])}});
                    }
            }
        }
        // In the order of the source list.
        std::sort(matches.begin(), matches.end(),
                  [](const QVariant &a, const QVariant &b) { return a.toMap().value("order").toInt() < b.toMap().value("order").toInt(); });
        const QVariantMap result{{"matches", matches}};
        if (!hid.isEmpty() && !matches.isEmpty())
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
            cachedDetails = Details{o.value("title").toString(), o.value("cover").toString(),
                                    o.value("description").toString(), o.value("author").toString(),
                                    o.value("status").toString(), o.value("genres").toVariant().toStringList()};
        }

        QList<std::function<QVariant()>> jobs;
        jobs.append([source, url]() -> QVariant { return chaptersToUnits(source->chapters(url)); });
        if (!cachedDetails)
            jobs.append([source, url]() -> QVariant {
                const auto d = source->details(url);
                if (!d)
                    return {};
                return QVariantMap{{"title", d->title},   {"cover", d->cover},   {"description", d->description},
                                   {"author", d->author}, {"status", d->status}, {"genres", d->genres}};
            });
        const auto results = parallel(jobs);

        QVariantMap details;
        if (cachedDetails)
            details = QVariantMap{{"title", cachedDetails->title},       {"cover", cachedDetails->cover},
                                  {"description", cachedDetails->description}, {"author", cachedDetails->author},
                                  {"status", cachedDetails->status},     {"genres", cachedDetails->genres}};
        else if (results.size() > 1 && results[1].ok() && results[1].value->isValid()) {
            details = results[1].value->toMap();
            Database::cachePut(detailsKey, QJsonDocument::fromVariant(details).toJson(QJsonDocument::Compact), 86400);
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
