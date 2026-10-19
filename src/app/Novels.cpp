#include "Novels.h"

#include "App.h"
#include "Comics.h"
#include "Downloads.h"
#include "Library.h"
#include "Resolve.h"
#include "core/Database.h"
#include "core/Parallel.h"
#include "core/Text.h"
#include "sources/novels/Registry.h"

#include <QJsonDocument>

namespace sc {

using namespace sc::novels;

namespace {

std::shared_ptr<Source> requireSource(const QString &url) {
    auto source = Registry::forUrl(url);
    if (!source)
        throw Error(QStringLiteral("SCANCORE does not support %1 yet.").arg(QUrl(url).host()));
    return source;
}

QVariantMap bookMap(const Book &book, const QString &label) {
    auto map = book.toMap();
    map.insert(QStringLiteral("id"), Library::idFor(QStringLiteral("novel"), book.source, book.url));
    map.insert(QStringLiteral("media"), QStringLiteral("novel"));
    map.insert(QStringLiteral("sourceLabel"), label);
    return map;
}

QVariantList unitsFrom(const QList<ChapterRef> &chapters) {
    QVariantList units;
    for (const auto &c : chapters)
        units.append(QVariantMap{{"key", c.url}, {"url", c.url}, {"title", c.title}, {"number", c.number}});
    return units;
}

} // namespace

QVariantList Novels::sources() const {
    QVariantList out;
    for (const auto &s : Registry::all())
        out.append(QVariantMap{{"key", s->key()},
                               {"label", s->label()},
                               {"enabled", s->config().enabled},
                               {"searchable", s->config().searchable},
                               {"disabledReason", s->config().disabledReason},
                               {"base", s->config().base},
                               {"extended", s->config().options.value("extended").toBool()},
                               {"language", s->config().language},
                               {"languageName", languageName(s->config().language)}});
    return out;
}

QString Novels::seriesId(const QString &url) const {
    auto source = Registry::forUrl(url);
    return Library::idFor(QStringLiteral("novel"), source ? source->key() : QString(), url);
}

Task *Novels::discover() {
    return Task::run([]() -> QVariant {
        const auto key = QStringLiteral("novel-discover");
        if (auto cached = Database::cacheGet(key))
            return QJsonDocument::fromJson(*cached).toVariant();
        QVariantList shelves;
        QString error;
        for (const auto &source : Registry::all()) {
            if (!source->config().enabled || !source->hasDiscover())
                continue;
            try {
                for (const auto &shelf : source->discover()) {
                    QVariantList books;
                    for (const auto &b : shelf.books)
                        books.append(bookMap(b, source->label()));
                    shelves.append(QVariantMap{{"title", shelf.title}, {"source", source->label()}, {"books", books}});
                }
            } catch (const Error &e) {
                error = e.message();
            }
        }
        if (shelves.isEmpty()) {
            if (auto stale = Database::cacheGet(key, true))
                return QJsonDocument::fromJson(*stale).toVariant();
            throw Error(error.isEmpty() ? QStringLiteral("Novel discovery is unavailable right now.") : error);
        }
        QVariantMap result{{"shelves", shelves}};
        Database::cachePut(key, QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact), 1800);
        return result;
    });
}

Task *Novels::catalog(const QString &query) {
    const auto q = query.simplified();
    const bool mature = App::instance()->settings().value(QStringLiteral("showNsfwSources")).toBool();
    return Task::run([q, mature]() -> QVariant { return Comics::catalogSearch(QStringLiteral("novel"), q, mature); });
}

Task *Novels::catalogFeeds(bool mature) {
    return Task::run([mature]() -> QVariant { return Comics::catalogShelves(QStringLiteral("novel"), mature); });
}

Task *Novels::resolve(const QVariantMap &item) {
    return Task::run([item]() -> QVariant {
        const auto names = Comics::catalogNames(item);
        QList<std::shared_ptr<Source>> sources;
        for (const auto &s : Registry::all())
            if (s->config().enabled && s->config().searchable && s->key() != QLatin1String("local") &&
                !s->config().options.value("extended").toBool())
                sources.append(s);
        return QVariantMap{{"matches", resolveAcross<std::shared_ptr<Source>>(sources, names)},
                           {"contentLanguage", contentLanguage()}};
    });
}

Task *Novels::search(const QString &query, const QString &only) {
    return Task::run([query = query.simplified(), only]() -> QVariant {
        // Books on this device are listed without a search.
        if (query.isEmpty() && only != QLatin1String("local"))
            return QVariantMap{{"books", QVariantList()}, {"providers", QVariantList()}};
        // A pasted book link opens that book directly.
        if (query.startsWith(QLatin1String("http")))
            if (auto source = Registry::forUrl(query)) {
                QList<ChapterRef> chapters;
                const auto book = source->book(query, chapters);
                return QVariantMap{{"books", QVariantList{bookMap(book, source->label())}}, {"providers", QVariantList()}};
            }
        QList<std::shared_ptr<Source>> selected;
        for (const auto &s : Registry::all())
            if (s->config().enabled && s->config().searchable &&
                (only == QLatin1String("all") ? !s->config().options.value("extended").toBool() : s->key() == only))
                selected.append(s);
        QList<std::function<QList<Book>()>> jobs;
        for (const auto &s : selected)
            jobs.append([s, query] { return s->search(query); });
        const auto results = parallel(jobs);
        const auto q = normalizedTitle(query);
        const auto words = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        struct Ranked {
            int tier;
            bool preferred;
            int order;
            QVariantMap book;
        };
        const auto wantedLanguage = contentLanguage();
        QList<Ranked> ranked;
        QVariantList providers;
        bool anyOk = false;
        for (int i = 0; i < results.size(); ++i) {
            providers.append(QVariantMap{{"source", selected[i]->key()},
                                         {"label", selected[i]->label()},
                                         {"ok", results[i].ok()},
                                         {"count", results[i].ok() ? results[i].value->size() : 0},
                                         {"error", results[i].error},
                                         {"verifyUrl", results[i].verifyUrl}});
            if (!results[i].ok())
                continue;
            anyOk = true;
            int order = 0;
            for (const auto &b : *results[i].value) {
                const auto t = normalizedTitle(b.title);
                // Sites that ignore the query return their default ranking;
                // keep only titles that mention every searched word.
                const bool matchesAll =
                    std::all_of(words.begin(), words.end(), [&](const QString &w) { return t.contains(w); });
                if (!matchesAll)
                    continue;
                const int tier = t == q ? 0 : t.startsWith(q) ? 1 : 2;
                ranked.append({tier, baseLanguage(selected[i]->config().language) == wantedLanguage, order++,
                               bookMap(b, selected[i]->label())});
            }
        }
        if (!anyOk && !selected.isEmpty())
            throw Error(QStringLiteral("Novel sources are unavailable right now. Check your connection and try again."));
        // Equally good titles in the content language first.
        std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
            if (a.tier != b.tier)
                return a.tier < b.tier;
            if (a.preferred != b.preferred)
                return a.preferred;
            return a.order < b.order;
        });
        QVariantList books;
        for (const auto &r : ranked)
            books.append(r.book);
        return QVariantMap{{"books", books}, {"providers", providers}};
    });
}

QVariantList Novels::fetchUnits(const QString &url) {
    QList<ChapterRef> chapters;
    requireSource(url)->book(url, chapters);
    return unitsFrom(chapters);
}

Task *Novels::book(const QString &url, const QVariantMap &summary) {
    return Task::run([url, summary]() -> QVariant {
        auto source = requireSource(url);
        const auto id = Library::idFor(QStringLiteral("novel"), source->key(), url);
        QVariantMap series = summary;
        series.insert(QStringLiteral("id"), id);
        series.insert(QStringLiteral("media"), QStringLiteral("novel"));
        series.insert(QStringLiteral("source"), source->key());
        series.insert(QStringLiteral("sourceLabel"), source->label());
        series.insert(QStringLiteral("sourceUrl"), url);
        try {
            QList<ChapterRef> chapters;
            const auto book = source->book(url, chapters);
            const auto fresh = bookMap(book, source->label());
            for (auto it = fresh.begin(); it != fresh.end(); ++it)
                if (!it.value().toString().isEmpty() || it.value().typeId() == QMetaType::QStringList)
                    series.insert(it.key(), it.value());
            series.insert(QStringLiteral("id"), id);
            series.insert(QStringLiteral("sourceUrl"), url);
            const auto units = unitsFrom(chapters);
            Library::upsertSeries(series);
            Library::storeUnits(id, units);
            return QVariantMap{{"series", series}, {"chapters", units}, {"offline", false}};
        } catch (const Error &e) {
            const auto cached = Library::cachedUnits(id);
            if (cached.isEmpty())
                throw;
            const auto stored = Library::instance()->seriesRow(id);
            for (auto it = stored.begin(); it != stored.end(); ++it)
                if (series.value(it.key()).toString().isEmpty())
                    series.insert(it.key(), it.value());
            return QVariantMap{{"series", series}, {"chapters", cached}, {"offline", true}, {"error", e.message()}};
        }
    });
}

QVariantMap Novels::fetchChapter(const QString &chapterUrl) {
    const auto cacheKey = QStringLiteral("novel-chapter:") + chapterUrl;
    if (auto cached = Database::cacheGet(cacheKey))
        return QJsonDocument::fromJson(*cached).toVariant().toMap();
    QString title;
    const auto blocks = requireSource(chapterUrl)->chapter(chapterUrl, title);
    QVariantList list;
    for (const auto &b : blocks)
        list.append(QVariantMap{{"kind", b.kind}, {"text", b.text}, {"html", b.html}});
    QVariantMap result{{"title", title}, {"blocks", list}};
    // Chapter text rarely changes; keeping it lets recently read chapters
    // open instantly and offline.
    Database::cachePut(cacheKey, QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact), 14 * 86400);
    return result;
}

Task *Novels::chapter(const QString &chapterUrl, const QString &seriesId) {
    return Task::run([chapterUrl, seriesId]() -> QVariant {
        if (!seriesId.isEmpty()) {
            auto local = Downloads::localNovelChapter(seriesId, chapterUrl);
            if (!local.isEmpty()) {
                local.insert(QStringLiteral("offline"), true);
                return local;
            }
        }
        auto result = fetchChapter(chapterUrl);
        result.insert(QStringLiteral("offline"), false);
        return result;
    });
}

} // namespace sc
