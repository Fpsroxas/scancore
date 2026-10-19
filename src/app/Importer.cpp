#include "Importer.h"

#include "Comics.h"
#include "Library.h"
#include "Novels.h"
#include "Trackers.h"
#include "core/Database.h"
#include "core/Parallel.h"
#include "core/Proto.h"
#include "core/Text.h"
#include "sources/comics/Registry.h"
#include "sources/local/Local.h"
#include "sources/novels/Registry.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>

#include <cstring>

namespace sc {

namespace {

const QString kUnmatched = QStringLiteral("import.unmatched");
QMutex g_unmatchedMutex;

// One series from elsewhere.
struct Item {
    QString media; // comic, novel or anime
    QStringList titles;
    QString cover;
    QString category = QStringLiteral("reading");
    QString sourceName; // Mihon's name for its source
    QString url;        // Mihon's address on that source (often a path)
    int anilistId = 0;
    int malId = 0;
    QString comickHid;
    // Chapters or episodes done: the numbers when known (Mihon), else a count.
    QList<double> numbers;
    int count = 0;
    qint64 updatedAt = 0;
    QJsonObject anime; // AniList media, for anime

    QString title() const { return titles.value(0); }
};

QVariantMap toMap(const Item &item) {
    QVariantList numbers;
    for (const double n : item.numbers)
        numbers.append(n);
    return {{"title", item.title()}, {"titles", item.titles},       {"media", item.media},     {"cover", item.cover},
            {"category", item.category}, {"source", item.sourceName}, {"anilistId", item.anilistId}, {"malId", item.malId},
            {"comick", item.comickHid},
            {"numbers", numbers},        {"count", item.count},       {"read", item.numbers.isEmpty() ? item.count : int(item.numbers.size())}};
}

Item fromMap(const QVariantMap &map) {
    Item item;
    item.media = map.value("media").toString();
    item.titles = map.value("titles").toStringList();
    if (item.titles.isEmpty())
        item.titles = {map.value("title").toString()};
    item.cover = map.value("cover").toString();
    item.category = map.value("category").toString();
    item.sourceName = map.value("source").toString();
    item.anilistId = map.value("anilistId").toInt();
    item.malId = map.value("malId").toInt();
    item.comickHid = map.value("comick").toString();
    for (const auto &n : map.value("numbers").toList())
        item.numbers.append(n.toDouble());
    item.count = map.value("count").toInt();
    return item;
}

// Mihon's Backup: manga (1) and the sources they come from (101).
QList<Item> parseMihon(const QByteArray &data) {
    QHash<quint64, QString> sourceNames;
    QList<std::pair<quint64, Item>> manga;
    Proto backup(data);
    while (backup.next()) {
        if (backup.field() == 101) {
            Proto source(backup.bytes());
            QString name;
            quint64 id = 0;
            while (source.next()) {
                if (source.field() == 1)
                    name = source.text();
                else if (source.field() == 2)
                    id = source.varint();
                else
                    source.skip();
            }
            sourceNames.insert(id, name);
        } else if (backup.field() == 1) {
            Proto m(backup.bytes());
            Item item;
            item.media = QStringLiteral("comic");
            quint64 source = 0;
            bool favorite = true;
            float tracked = 0;
            QString title;
            while (m.next()) {
                switch (m.field()) {
                case 1: source = m.varint(); break;
                case 2: item.url = m.text(); break;
                case 3: title = m.text(); break;
                case 9: item.cover = m.text(); break;
                case 16: {
                    Proto chapter(m.bytes());
                    bool read = false;
                    double number = -1;
                    qint64 changed = 0;
                    while (chapter.next()) {
                        if (chapter.field() == 4)
                            read = chapter.varint() != 0;
                        else if (chapter.field() == 9)
                            number = chapter.number();
                        else if (chapter.field() == 11)
                            changed = qint64(chapter.varint());
                        else
                            chapter.skip();
                    }
                    if (read && number >= 0) {
                        item.numbers.append(number);
                        item.updatedAt = std::max(item.updatedAt, changed);
                    }
                    break;
                }
                case 18: {
                    Proto tracking(m.bytes());
                    int service = 0;
                    qint64 id = 0;
                    float last = 0;
                    while (tracking.next()) {
                        if (tracking.field() == 1)
                            service = int(tracking.varint());
                        else if (tracking.field() == 3 || tracking.field() == 100)
                            id = std::max<qint64>(id, qint64(tracking.varint()));
                        else if (tracking.field() == 6)
                            last = float(tracking.number());
                        else
                            tracking.skip();
                    }
                    // Mihon's tracker ids: 1 MyAnimeList, 2 AniList.
                    if (service == 1)
                        item.malId = int(id);
                    else if (service == 2)
                        item.anilistId = int(id);
                    tracked = std::max(tracked, last);
                    break;
                }
                case 100: favorite = m.varint() != 0; break;
                default: m.skip();
                }
            }
            if (m.bad())
                throw Error(QStringLiteral("This backup is damaged."));
            // Entries outside the library only carry history.
            if (!favorite || title.isEmpty())
                continue;
            item.titles = {title};
            item.count = int(tracked);
            manga.append({source, item});
        } else {
            backup.skip();
        }
    }
    if (backup.bad())
        throw Error(QStringLiteral("This is not a Mihon or Tachiyomi backup, or it is damaged."));
    QList<Item> out;
    for (auto &[source, item] : manga) {
        item.sourceName = sourceNames.value(source);
        out.append(item);
    }
    return out;
}

// ------------------------------------------------------------ AniList

QJsonObject anilist(const QString &query, const QJsonObject &variables) {
    const auto response = Http::postJson(QUrl(QStringLiteral("https://graphql.anilist.co")),
                                         QJsonDocument(QJsonObject{{"query", query}, {"variables", variables}}).toJson(QJsonDocument::Compact),
                                         {{"Accept", "application/json"}});
    const auto json = response.json().object();
    if (!response.ok()) {
        const auto message = json.value("errors").toArray().first().toObject().value("message").toString();
        if (response.status == 404)
            throw Error(QStringLiteral("AniList has no public list by that name."), 404);
        if (response.status == 429)
            throw Error(QStringLiteral("AniList is busy. Try again in a minute."), 429);
        throw Error(message.isEmpty() ? QStringLiteral("AniList answered %1.").arg(response.status) : QStringLiteral("AniList: ") + message,
                    response.status);
    }
    return json.value("data").toObject();
}

const char *kMediaFields = R"(
  id idMal type format status(version: 2) episodes chapters averageScore seasonYear startDate { year }
  title { romaji english native userPreferred } synonyms description(asHtml: false) genres bannerImage
  coverImage { extraLarge large color })";

QStringList titlesOf(const QJsonObject &media) {
    const auto t = media.value("title").toObject();
    QStringList titles{t.value("english").toString(), t.value("romaji").toString(), t.value("userPreferred").toString(),
                       t.value("native").toString()};
    for (const auto &s : media.value("synonyms").toArray())
        titles.append(s.toString());
    titles.removeAll(QString());
    titles.removeDuplicates();
    return titles;
}

QString categoryFor(const QString &status) {
    const auto s = status.toUpper().remove(QLatin1Char(' ')).remove(QLatin1Char('-'));
    if (s == QLatin1String("COMPLETED") || s == QLatin1String("2"))
        return QStringLiteral("completed");
    if (s == QLatin1String("DROPPED") || s == QLatin1String("4"))
        return QStringLiteral("dropped");
    if (s == QLatin1String("PLANNING") || s.startsWith(QLatin1String("PLANTO")) || s == QLatin1String("6"))
        return QStringLiteral("planned");
    return QStringLiteral("reading");
}

Item itemFrom(const QJsonObject &media) {
    Item item;
    const auto format = media.value("format").toString();
    item.media = media.value("type").toString() == QLatin1String("ANIME") ? QStringLiteral("anime")
                 : format == QLatin1String("NOVEL")                       ? QStringLiteral("novel")
                                                                          : QStringLiteral("comic");
    item.titles = titlesOf(media);
    item.cover = media.value("coverImage").toObject().value("extraLarge").toString(
        media.value("coverImage").toObject().value("large").toString());
    item.anilistId = media.value("id").toInt();
    item.malId = media.value("idMal").toInt();
    if (item.media == QLatin1String("anime"))
        item.anime = media;
    return item;
}

// AniList media for MyAnimeList ids, 50 at a time.
QHash<int, QJsonObject> byMalIds(const QList<int> &ids, bool anime) {
    QHash<int, QJsonObject> out;
    for (int i = 0; i < ids.size(); i += 50) {
        QJsonArray chunk;
        for (const int id : ids.mid(i, 50))
            chunk.append(id);
        const auto data = anilist(QStringLiteral("query ($ids: [Int], $type: MediaType) { Page(perPage: 50) { media(idMal_in: $ids, type: $type) {%1} } }")
                                      .arg(QString::fromLatin1(kMediaFields)),
                                  {{"ids", chunk}, {"type", anime ? "ANIME" : "MANGA"}});
        for (const auto &value : data.value("Page").toObject().value("media").toArray())
            out.insert(value.toObject().value("idMal").toInt(), value.toObject());
    }
    return out;
}

// ------------------------------------------------------------ MyAnimeList

QList<Item> parseMal(const QByteArray &data) {
    QList<Item> out;
    QXmlStreamReader xml(data);
    Item item;
    bool inEntry = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const auto name = xml.name();
            if (name == QLatin1String("anime") || name == QLatin1String("manga")) {
                item = Item();
                item.media = name == QLatin1String("anime") ? QStringLiteral("anime") : QStringLiteral("comic");
                inEntry = true;
            } else if (inEntry) {
                const auto value = xml.readElementText().trimmed();
                if (name == QLatin1String("series_animedb_id") || name == QLatin1String("manga_mangadb_id"))
                    item.malId = value.toInt();
                else if (name == QLatin1String("series_title") || name == QLatin1String("manga_title"))
                    item.titles = {value};
                else if (name == QLatin1String("my_watched_episodes") || name == QLatin1String("my_read_chapters"))
                    item.count = value.toInt();
                else if (name == QLatin1String("my_status"))
                    item.category = categoryFor(value);
            }
        } else if (xml.isEndElement() && (xml.name() == QLatin1String("anime") || xml.name() == QLatin1String("manga"))) {
            if (inEntry && item.malId > 0)
                out.append(item);
            inEntry = false;
        }
    }
    if (xml.hasError() && out.isEmpty())
        throw Error(QStringLiteral("This is not a MyAnimeList export."));
    return out;
}

// ------------------------------------------------------------ saving

struct Tally {
    int imported = 0;
    int progress = 0;
    QList<Item> unmatched;
    QStringList saved;
};

// Into the library (when not there yet), with its chapters or episodes done.
int store(const QVariantMap &series, const QString &category, const QVariantList &units, const Item &item) {
    const auto id = series.value("id").toString();
    const auto when = item.updatedAt > 0 ? std::min(item.updatedAt, Database::now()) : Database::now();
    QSet<qint64> numbers;
    for (const double n : item.numbers)
        numbers.insert(qRound64(n * 1000));
    int marked = 0;
    Library::upsertSeries(series);
    if (!units.isEmpty() && series.value("media").toString() != QLatin1String("anime"))
        Library::storeUnits(id, units);
    Database::transaction([&] {
        Database::query(QStringLiteral("INSERT OR IGNORE INTO library(series_id, category, added_at, known_units, changed_at) VALUES(?, ?, ?, ?, ?)"),
                        {id, category, when, units.size(), Database::now()});
        Database::query(QStringLiteral("DELETE FROM library_removed WHERE series_id = ?"), {id});
        for (const auto &value : units) {
            const auto unit = value.toMap();
            bool ok = false;
            const double number = unit.value("number").toString().toDouble(&ok);
            if (!ok)
                continue;
            const bool done = numbers.isEmpty() ? number <= item.count && number > 0 : numbers.contains(qRound64(number * 1000));
            if (!done)
                continue;
            Database::query(QStringLiteral(R"(
                INSERT INTO progress(series_id, unit_key, unit_title, unit_number, unit_url, percent, completed, updated_at)
                VALUES(?, ?, ?, ?, ?, 1, 1, ?)
                ON CONFLICT(series_id, unit_key) DO UPDATE SET completed = 1, percent = 1
            )"),
                            {id, unit.value("key").toString(), unit.value("title").toString(), unit.value("number").toString(),
                             unit.value("url").toString(), when});
            ++marked;
        }
    });
    if (item.anilistId > 0 && series.value("media").toString() != QLatin1String("anime"))
        Trackers::remember(id, QStringLiteral("anilist"), item.anilistId, item.title());
    if (item.malId > 0)
        Trackers::remember(id, QStringLiteral("mal"), item.malId, item.title());
    if (!item.comickHid.isEmpty())
        Trackers::remember(id, QStringLiteral("comick"), item.comickHid, item.title());
    return marked;
}

void storeAnime(const Item &item, Tally &outcome) {
    const auto &m = item.anime;
    const auto cover = m.value("coverImage").toObject();
    const int id = m.value("id").toInt();
    QVariantMap extra{{"anilistId", id},
                      {"banner", m.value("bannerImage").toString()},
                      {"color", cover.value("color").toString()},
                      {"format", m.value("format").toString()},
                      {"episodes", m.value("episodes").toInt()},
                      {"score", m.value("averageScore").toInt()},
                      {"year", m.value("seasonYear").toInt() ? m.value("seasonYear").toInt() : m.value("startDate").toObject().value("year").toInt()}};
    const QVariantMap series{{"id", Library::idFor(QStringLiteral("anime"), QStringLiteral("anilist"), QString::number(id))},
                             {"media", QStringLiteral("anime")},
                             {"source", QStringLiteral("anilist")},
                             {"sourceUrl", QStringLiteral("https://anilist.co/anime/%1").arg(id)},
                             {"title", item.title()},
                             {"cover", item.cover},
                             {"description", m.value("description").toString().remove(QRegularExpression(QStringLiteral("<[^>]+>")))},
                             {"genres", m.value("genres").toVariant()},
                             {"extra", extra}};
    // Episodes are keyed by their number.
    QVariantList episodes;
    for (int n = 1; n <= item.count; ++n)
        episodes.append(QVariantMap{{"key", QString::number(n)}, {"number", QString::number(n)}, {"title", QStringLiteral("Episode %1").arg(n)}});
    outcome.progress += store(series, item.category, episodes, item);
    outcome.saved.append(series.value("id").toString());
    ++outcome.imported;
}

template <typename SourcePtr>
bool sameSource(const SourcePtr &source, const QString &name) {
    const auto a = titleKey(source->label());
    const auto b = titleKey(name);
    return !a.isEmpty() && !b.isEmpty() && (a == b || a.startsWith(b) || b.startsWith(a));
}

// A comic or novel on one of SCANCORE's sources, saved with its progress;
// false when no source has it.
bool storeFound(const Item &item, Tally &outcome, bool searchEverywhere) {
    const bool novel = item.media == QLatin1String("novel");
    QStringList wanted;
    for (const auto &t : item.titles)
        if (!titleKey(t).isEmpty())
            wanted.append(titleKey(t));
    QVariantMap series;
    QVariantList units;

    const auto tryUrl = [&](const QString &url, const QString &sourceKey, const QString &title, const QString &cover) {
        try {
            units = novel ? Novels::fetchUnits(url) : Comics::fetchUnits(url);
        } catch (const Error &) {
            return false;
        }
        if (units.isEmpty())
            return false;
        series = {{"id", Library::idFor(item.media, sourceKey, url)}, {"media", item.media}, {"source", sourceKey},
                  {"sourceUrl", url}, {"title", title.isEmpty() ? item.title() : title}, {"cover", cover.isEmpty() ? item.cover : cover}};
        return true;
    };

    if (!novel) {
        for (const auto &source : comics::Registry::all()) {
            if (!source->enabled() || source->key() == QLatin1String("local"))
                continue;
            const bool named = !item.sourceName.isEmpty() && sameSource(source, item.sourceName);
            if (!named && !searchEverywhere)
                continue;
            // Mihon keeps the address on the site, usually without the site.
            if (named && !item.url.isEmpty()) {
                auto url = item.url.startsWith(QLatin1String("http")) ? item.url : source->config().base + item.url;
                if (source->key() == QLatin1String("mangadex")) // Mihon: /manga/<id>, the site: /title/<id>
                    url.replace(QLatin1String("/manga/"), QLatin1String("/title/"));
                if (comics::Registry::forUrl(url) == source && tryUrl(url, source->key(), {}, {}))
                    break;
            }
            QList<comics::Summary> found;
            try {
                found = source->search(item.title());
            } catch (const Error &) {
                continue;
            }
            bool done = false;
            for (const auto &summary : found)
                if (wanted.contains(titleKey(summary.title)) && tryUrl(summary.url, source->key(), summary.title, summary.cover)) {
                    done = true;
                    break;
                }
            if (done)
                break;
        }
    } else {
        for (const auto &source : novels::Registry::all()) {
            if (!source->config().enabled || !source->config().searchable || source->key() == QLatin1String("local"))
                continue;
            if (!searchEverywhere && !(item.sourceName.size() && sameSource(source, item.sourceName)))
                continue;
            QList<novels::Book> found;
            try {
                found = source->search(item.title());
            } catch (const Error &) {
                continue;
            }
            bool done = false;
            for (const auto &book : found)
                if (wanted.contains(titleKey(book.title)) && tryUrl(book.url, source->key(), book.title, book.cover)) {
                    done = true;
                    break;
                }
            if (done)
                break;
        }
    }
    if (series.isEmpty())
        return false;
    outcome.progress += store(series, item.category, units, item);
    outcome.saved.append(series.value("id").toString());
    ++outcome.imported;
    return true;
}

// Comics and novels, a few at a time so no site gets flooded.
void storeAll(const QList<Item> &items, Tally &outcome, bool searchEverywhere) {
    for (int i = 0; i < items.size(); i += 4) {
        if (CancelScope::cancelled())
            throw Error(QStringLiteral("Cancelled"), 499);
        QList<std::function<Tally()>> jobs;
        for (const auto &item : items.mid(i, 4))
            jobs.append([item, searchEverywhere] {
                Tally one;
                if (!storeFound(item, one, searchEverywhere))
                    one.unmatched.append(item);
                return one;
            });
        for (const auto &result : parallel(jobs)) {
            if (!result.ok())
                continue;
            outcome.imported += result.value->imported;
            outcome.progress += result.value->progress;
            outcome.saved += result.value->saved;
            outcome.unmatched += result.value->unmatched;
        }
    }
}

void addUnmatched(const QList<Item> &items) {
    QMutexLocker lock(&g_unmatchedMutex);
    auto list = Database::pref(kUnmatched).toList();
    QSet<QString> known;
    for (const auto &value : list)
        known.insert(value.toMap().value("media").toString() + titleKey(value.toMap().value("title").toString()));
    for (const auto &item : items)
        if (!known.contains(item.media + titleKey(item.title()))) {
            list.append(toMap(item));
            known.insert(item.media + titleKey(item.title()));
        }
    Database::setPref(kUnmatched, list);
}

QVariantMap summary(const Tally &outcome) {
    addUnmatched(outcome.unmatched);
    return {{"imported", outcome.imported},
            {"unmatched", outcome.unmatched.size()},
            {"progress", outcome.progress},
            {"saved", outcome.saved}};
}

QByteArray readFile(const QUrl &file) {
    QFile in(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!in.open(QIODevice::ReadOnly))
        throw Error(QStringLiteral("Could not open %1.").arg(in.fileName()));
    if (in.size() > 200 * 1024 * 1024)
        throw Error(QStringLiteral("That file is too large to import."));
    auto data = in.readAll();
    if (data.startsWith("\x1f\x8b")) {
        data = local::gunzip(data);
        if (data.isEmpty())
            throw Error(QStringLiteral("That file could not be unpacked."));
    }
    return data;
}

} // namespace

Importer::Importer(QObject *parent) : QObject(parent) {}

Importer *Importer::instance() {
    static auto *importer = [] {
        auto *created = new Importer;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return importer;
}

QVariantList Importer::unmatched() const {
    QMutexLocker lock(&g_unmatchedMutex);
    return Database::pref(kUnmatched).toList();
}

Task *Importer::finish(Task *task) {
    task->then(this, [this](const QVariant &value) {
        const auto saved = value.toMap().value("saved").toStringList();
        for (const auto &id : saved)
            emit Library::instance()->saved(id);
        if (!saved.isEmpty())
            Library::instance()->notifyChanged();
        emit changed();
    });
    return task;
}

Task *Importer::importMihon(const QUrl &file) {
    return finish(Task::run([file]() -> QVariant {
        const auto items = parseMihon(readFile(file));
        if (items.isEmpty())
            throw Error(QStringLiteral("This backup has no library."));
        Tally outcome;
        // Only the sources the backup names; the rest wait for matchAll.
        storeAll(items, outcome, false);
        return summary(outcome);
    }));
}

Task *Importer::importAniList(const QString &user) {
    const auto name = user.trimmed().section(QLatin1Char('/'), -1, -1, QString::SectionSkipEmpty);
    return finish(Task::run([name]() -> QVariant {
        if (name.isEmpty())
            throw Error(QStringLiteral("Type your AniList user name."));
        Tally outcome;
        QList<Item> books;
        for (const auto *type : {"ANIME", "MANGA"}) {
            const auto data = anilist(QStringLiteral("query ($user: String, $type: MediaType) { MediaListCollection(userName: $user, type: $type) {"
                                                     " lists { entries { status progress updatedAt media {%1} } } } }")
                                          .arg(QString::fromLatin1(kMediaFields)),
                                      {{"user", name}, {"type", type}});
            for (const auto &list : data.value("MediaListCollection").toObject().value("lists").toArray())
                for (const auto &value : list.toObject().value("entries").toArray()) {
                    const auto entry = value.toObject();
                    auto item = itemFrom(entry.value("media").toObject());
                    item.category = categoryFor(entry.value("status").toString());
                    item.count = entry.value("progress").toInt();
                    item.updatedAt = entry.value("updatedAt").toInteger();
                    if (item.media == QLatin1String("anime"))
                        storeAnime(item, outcome);
                    else
                        books.append(item);
                }
        }
        // Comics and novels have no source yet; matchAll looks for them.
        outcome.unmatched += books;
        return summary(outcome);
    }));
}

Task *Importer::importMal(const QUrl &file) {
    return finish(Task::run([file]() -> QVariant {
        const auto items = parseMal(readFile(file));
        if (items.isEmpty())
            throw Error(QStringLiteral("This export has no titles."));
        QList<int> animeIds;
        QList<int> mangaIds;
        for (const auto &item : items)
            (item.media == QLatin1String("anime") ? animeIds : mangaIds).append(item.malId);
        const auto anime = byMalIds(animeIds, true);
        const auto manga = byMalIds(mangaIds, false);
        Tally outcome;
        for (auto item : items) {
            const auto media = (item.media == QLatin1String("anime") ? anime : manga).value(item.malId);
            if (!media.isEmpty()) {
                auto found = itemFrom(media);
                found.category = item.category;
                found.count = item.count;
                item = found;
            }
            if (item.media == QLatin1String("anime")) {
                if (!item.anime.isEmpty())
                    storeAnime(item, outcome);
            } else {
                outcome.unmatched.append(item);
            }
        }
        return summary(outcome);
    }));
}

Task *Importer::importComick() {
    return finish(Task::run([]() -> QVariant {
        const auto entries = Trackers::comickLibrary();
        if (entries.isEmpty())
            throw Error(QStringLiteral("Your Comick library is empty."));
        Tally outcome;
        QList<Item> anime;
        static const QStringList categories{QString(), QStringLiteral("reading"), QStringLiteral("completed"), QStringLiteral("reading"),
                                            QStringLiteral("dropped"), QStringLiteral("planned")};
        for (const auto &value : entries) {
            const auto entry = value.toMap();
            Item item;
            item.media = entry.value("media_type").toString() == QLatin1String("anime") ? QStringLiteral("anime") : QStringLiteral("comic");
            item.titles = {entry.value("title").toString()};
            for (const auto &t : entry.value("titles").toList())
                item.titles.append(t.toMap().value("title").toString());
            item.titles.removeAll(QString());
            item.titles.removeDuplicates();
            item.cover = entry.value("cover_url").toString();
            item.category = categories.value(entry.value("status").toInt(), QStringLiteral("reading"));
            item.count = int(entry.value("progress").toMap().value("number").toString().toDouble());
            item.comickHid = entry.value("hid").toString();
            item.updatedAt = QDateTime::fromString(entry.value("progressed_at").toString(), Qt::ISODateWithMs).toSecsSinceEpoch();
            (item.media == QLatin1String("anime") ? anime : outcome.unmatched).append(item);
        }
        // Anime are found by their AniList entry, which Comick knows.
        QList<int> ids;
        QHash<int, Item> byId;
        for (auto &item : anime) {
            if (CancelScope::cancelled())
                throw Error(QStringLiteral("Cancelled"), 499);
            item.anilistId = Trackers::comickAniListId(item.comickHid);
            if (item.anilistId > 0) {
                ids.append(item.anilistId);
                byId.insert(item.anilistId, item);
            }
        }
        for (int i = 0; i < ids.size(); i += 50) {
            QJsonArray chunk;
            for (const int id : ids.mid(i, 50))
                chunk.append(id);
            const auto data = anilist(QStringLiteral("query ($ids: [Int]) { Page(perPage: 50) { media(id_in: $ids, type: ANIME) {%1} } }")
                                          .arg(QString::fromLatin1(kMediaFields)),
                                      {{"ids", chunk}});
            for (const auto &media : data.value("Page").toObject().value("media").toArray()) {
                auto item = byId.value(media.toObject().value("id").toInt());
                const auto found = itemFrom(media.toObject());
                item.anime = found.anime;
                item.malId = found.malId;
                if (!item.anime.isEmpty())
                    storeAnime(item, outcome);
            }
        }
        return summary(outcome);
    }));
}

Task *Importer::matchAll() {
    return finish(Task::run([]() -> QVariant {
        QList<Item> waiting;
        for (const auto &value : Importer::instance()->unmatched())
            waiting.append(fromMap(value.toMap()));
        Tally outcome;
        storeAll(waiting, outcome, true);
        {
            // Titles an import added meanwhile stay too.
            QMutexLocker lock(&g_unmatchedMutex);
            QSet<QString> tried;
            for (const auto &item : waiting)
                tried.insert(item.media + titleKey(item.title()));
            QVariantList left;
            for (const auto &value : Database::pref(kUnmatched).toList())
                if (!tried.contains(value.toMap().value("media").toString() + titleKey(value.toMap().value("title").toString())))
                    left.append(value);
            for (const auto &item : outcome.unmatched)
                left.append(toMap(item));
            Database::setPref(kUnmatched, left);
        }
        return QVariantMap{{"matched", outcome.imported}, {"left", outcome.unmatched.size()}, {"progress", outcome.progress},
                           {"saved", outcome.saved}};
    }));
}

void Importer::forget(const QString &title) {
    {
        QMutexLocker lock(&g_unmatchedMutex);
        auto list = Database::pref(kUnmatched).toList();
        list.removeIf([&](const QVariant &value) { return value.toMap().value("title").toString() == title; });
        Database::setPref(kUnmatched, list);
    }
    emit changed();
}

void Importer::forgetAll() {
    {
        QMutexLocker lock(&g_unmatchedMutex);
        Database::setPref(kUnmatched, QVariantList());
    }
    emit changed();
}

} // namespace sc
