// Comic sources that publish a JSON API: Asura, MangaDex, Iken (Vortex),
// HiveToons and Qi Manga.

#include "Engines.h"

#include "core/Html.h"

#include <QDateTime>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QThread>

#include <mutex>

namespace sc::comics {

namespace {

QJsonArray arr(const QJsonValue &v) { return v.toArray(); }
QJsonObject obj(const QJsonValue &v) { return v.toObject(); }

QUrl apiUrl(const QString &base, const QString &path, const QList<QPair<QString, QString>> &params = {}) {
    QUrl url(base + path);
    QUrlQuery query(url);
    for (const auto &[key, value] : params)
        query.addQueryItem(QString::fromUtf8(QUrl::toPercentEncoding(key)),
                           QString::fromUtf8(QUrl::toPercentEncoding(value)));
    url.setQuery(query);
    return url;
}

QStringList names(const QJsonArray &rows, const QString &field = QStringLiteral("name")) {
    QStringList out;
    for (const auto &row : rows) {
        const auto value = row.isObject() ? str(row.toObject().value(field)) : str(row);
        if (!value.isEmpty())
            out.append(value);
    }
    return out;
}

QString stripTags(const QString &html) {
    if (!html.contains(QLatin1Char('<')))
        return decodeEntities(html);
    HtmlDocument doc(html);
    QStringList paragraphs;
    for (const auto &p : doc.select(QStringLiteral("p")))
        if (auto text = p.textTrim(); !text.isEmpty())
            paragraphs.append(text);
    return paragraphs.isEmpty() ? doc.root().textTrim() : paragraphs.join(QStringLiteral("\n\n"));
}

} // namespace

// ---------------------------------------------------------------- Asura

namespace {
QString asuraSlug(const QString &seriesUrl) {
    const QUrl url(seriesUrl);
    const auto path = url.path();
    const auto segment = pathSegment(url, -1);
    if (path.startsWith(QLatin1String("/comics/"))) {
        static const QRegularExpression suffix(QStringLiteral("-[a-f0-9]{8}$"),
                                               QRegularExpression::CaseInsensitiveOption);
        return QString(segment).remove(suffix);
    }
    return segment;
}
} // namespace

QList<Summary> AsuraSource::search(const QString &query) {
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/search"),
                                          {{"q", query}, {"page", "1"}, {"per_page", "20"}}),
                                   jsonHeaders());
    QList<Summary> out;
    for (const auto &row : arr(doc.object().value("data"))) {
        const auto s = row.toObject();
        Summary item;
        item.title = str(s.value("title"));
        const auto publicUrl = str(s.value("public_url"));
        item.url = QUrl(m_config.base)
                       .resolved(QUrl(publicUrl.isEmpty() ? QStringLiteral("/s/") + str(s.value("id")) : publicUrl))
                       .toString();
        item.cover = str(s.value("cover"));
        if (item.cover.isEmpty())
            item.cover = str(s.value("cover_url"));
        item.type = QStringLiteral("Manhwa");
        item.source = key();
        if (!item.title.isEmpty())
            out.append(item);
    }
    return out;
}

QList<Chapter> AsuraSource::chapters(const QString &seriesUrl) {
    const auto slug = asuraSlug(seriesUrl);
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/series/%1/chapters").arg(slug),
                                          {{"page", "1"}, {"per_page", "2000"}, {"order", "asc"}}),
                                   jsonHeaders(), 20000);
    QList<Chapter> out;
    for (const auto &row : arr(doc.object().value("data"))) {
        const auto c = row.toObject();
        Chapter ch;
        ch.number = str(c.value("number"));
        ch.slug = str(c.value("slug"));
        const auto title = str(c.value("title"));
        ch.title = chapterLabel(ch.number, title);
        ch.url = QStringLiteral("%1/api/series/%2/chapters/%3").arg(m_config.api, slug, ch.slug);
        for (const char *field : {"created_at", "published_at", "createdAt", "publishedAt"})
            if (ch.released.isEmpty())
                ch.released = str(c.value(QLatin1String(field)));
        if (!ch.number.isEmpty() && !ch.slug.isEmpty())
            out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Panel> AsuraSource::panels(const QString &chapterUrl) {
    const auto doc = Http::getJson(QUrl(chapterUrl), jsonHeaders());
    QList<Panel> out;
    for (const auto &page : arr(obj(obj(doc.object().value("data")).value("chapter")).value("pages"))) {
        const auto p = page.toObject();
        Panel panel{str(p.value("url")), p.value("width").toInt(), p.value("height").toInt()};
        if (!panel.url.isEmpty())
            out.append(panel);
    }
    return out;
}

std::optional<Details> AsuraSource::details(const QString &seriesUrl) {
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/series/%1").arg(asuraSlug(seriesUrl))),
                                   jsonHeaders());
    auto s = doc.object().value("series").toObject();
    if (s.isEmpty())
        s = doc.object().value("data").toObject();
    if (s.contains("series"))
        s = s.value("series").toObject();
    if (s.isEmpty())
        return std::nullopt;
    Details d;
    d.title = str(s.value("title"));
    d.cover = str(s.value("cover"));
    if (d.cover.isEmpty())
        d.cover = str(s.value("cover_url"));
    for (const char *field : {"description", "synopsis", "summary"})
        if (d.description.isEmpty())
            d.description = stripTags(str(s.value(QLatin1String(field))));
    d.status = str(s.value("status"));
    d.genres = names(arr(s.value("genres")));
    d.author = str(s.value("author"));
    return d;
}

QList<LatestItem> AsuraSource::latest(int limit) {
    QList<LatestItem> out;
    for (int offset = 0; out.size() < limit && offset < 200; offset += 20) {
        const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/series"),
                                              {{"offset", QString::number(offset)},
                                               {"order", "updated_at"},
                                               {"sort", "desc"}}),
                                       jsonHeaders());
        const auto rows = arr(doc.object().value("data"));
        for (const auto &row : rows) {
            const auto s = row.toObject();
            const auto chapter = arr(s.value("latest_chapters")).first().toObject();
            LatestItem item;
            item.source = key();
            item.sourceLabel = label();
            item.title = str(s.value("title"));
            const auto publicUrl = str(s.value("public_url"));
            item.url = QUrl(m_config.base)
                           .resolved(QUrl(publicUrl.isEmpty() ? QStringLiteral("/s/") + str(s.value("id")) : publicUrl))
                           .toString();
            item.cover = str(s.value("cover"));
            if (item.cover.isEmpty())
                item.cover = str(s.value("cover_url"));
            item.chapterNumber = str(chapter.value("number"));
            if (!item.chapterNumber.isEmpty())
                item.chapterTitle = chapterLabel(item.chapterNumber);
            if (!str(chapter.value("slug")).isEmpty() && !str(s.value("slug")).isEmpty())
                item.chapterUrl = QStringLiteral("%1/api/series/%2/chapters/%3")
                                      .arg(m_config.api, str(s.value("slug")), str(chapter.value("slug")));
            for (const auto &value : {chapter.value("published_at"), s.value("last_chapter_at"), s.value("updated_at")})
                if (item.publishedAt.isEmpty())
                    item.publishedAt = str(value);
            if (!item.title.isEmpty())
                out.append(item);
        }
        if (rows.isEmpty() || !obj(doc.object().value("meta")).value("has_more").toBool())
            break;
    }
    return out;
}

// ---------------------------------------------------------------- MangaDex

namespace {
std::mutex g_dexMutex;
qint64 g_dexNext = 0;
qint64 g_dexNextAtHome = 0;

QString dexId(const QString &value, const QString &type) {
    const QUrl url(value);
    static const QRegularExpression uuid(
        QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"),
        QRegularExpression::CaseInsensitiveOption);
    if (!hostMatches(url.host(), QStringLiteral("mangadex.org")) || pathSegment(url, 0) != type ||
        !uuid.match(pathSegment(url, 1)).hasMatch())
        throw Error(QStringLiteral("That is not a MangaDex %1 link.").arg(type));
    return pathSegment(url, 1);
}

// The name in the first wanted language that has one (other languages'
// names are mostly among the alternative titles), then English, then any.
QString dexTitle(const QJsonObject &attrs, const QStringList &languages) {
    const auto titles = attrs.value("title").toObject();
    for (const auto &language : languages) {
        if (const auto title = str(titles.value(language)); !title.isEmpty())
            return title;
        // English keeps the main title, as before languages were added.
        if (language == QLatin1String("en"))
            continue;
        for (const auto &alt : arr(attrs.value("altTitles")))
            if (const auto title = str(alt.toObject().value(language)); !title.isEmpty())
                return title;
    }
    if (const auto title = str(titles.value("en")); !title.isEmpty())
        return title;
    return titles.isEmpty() ? QString() : str(titles.begin().value());
}

QString dexText(const QJsonObject &texts, const QStringList &languages) {
    for (const auto &language : languages + QStringList{QStringLiteral("en")})
        if (const auto text = str(texts.value(language)); !text.isEmpty())
            return text;
    for (const auto &text : texts)
        if (!str(text).isEmpty())
            return str(text);
    return {};
}
} // namespace

QStringList MangaDexSource::languages() const {
    QStringList out;
    for (const auto &value : m_config.options.value("languages").toArray())
        if (const auto language = str(value).trimmed(); !language.isEmpty() && !out.contains(language))
            out.append(language);
    return out.isEmpty() ? QStringList{QStringLiteral("en")} : out;
}

QJsonDocument MangaDexSource::api(const QString &path, const QList<QPair<QString, QString>> &params, bool atHome) {
    // MangaDex asks clients to stay under ~5 requests a second, and far fewer
    // at-home lookups. Requests are spaced instead of rejected.
    qint64 wait = 0;
    {
        std::lock_guard lock(g_dexMutex);
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const auto at = std::max({now, g_dexNext, atHome ? g_dexNextAtHome : qint64(0)});
        if (at - now > 8000)
            throw Error(QStringLiteral("MangaDex is busy. Try again in a moment."), 429);
        g_dexNext = at + 260;
        if (atHome)
            g_dexNextAtHome = at + 1600;
        wait = at - now;
    }
    while (wait > 0 && !CancelScope::cancelled()) {
        QThread::msleep(std::min<qint64>(wait, 50));
        wait -= 50;
    }
    return Http::getJson(apiUrl(m_config.api, path, params), {{"User-Agent", "ScanCore-Desktop/1.0"}});
}

QList<Summary> MangaDexSource::search(const QString &query) {
    const auto wanted = languages();
    QList<QPair<QString, QString>> params{{"title", query}, {"limit", "24"}, {"includes[]", "cover_art"}};
    for (const auto &language : wanted)
        params.append({"availableTranslatedLanguage[]", language});
    params.append({{"contentRating[]", "safe"}, {"contentRating[]", "suggestive"}, {"order[relevance]", "desc"}});
    const auto doc = api(QStringLiteral("/manga"), params);
    QList<Summary> out;
    for (const auto &row : arr(doc.object().value("data"))) {
        const auto manga = row.toObject();
        const auto attrs = manga.value("attributes").toObject();
        Summary item;
        item.title = dexTitle(attrs, wanted);
        const auto id = str(manga.value("id"));
        item.url = tagged(QStringLiteral("https://mangadex.org/title/%1").arg(id));
        for (const auto &rel : arr(manga.value("relationships"))) {
            const auto r = rel.toObject();
            if (str(r.value("type")) == QLatin1String("cover_art")) {
                const auto file = str(obj(r.value("attributes")).value("fileName"));
                if (!file.isEmpty())
                    item.cover = QStringLiteral("https://uploads.mangadex.org/covers/%1/%2.512.jpg").arg(id, file);
            }
        }
        QStringList tags;
        for (const auto &tag : arr(attrs.value("tags")))
            tags.append(str(obj(obj(tag.toObject().value("attributes")).value("name")).value("en")));
        tags.removeAll(QString());
        item.genres = tags.join(QStringLiteral(", "));
        item.type = QStringLiteral("Manga");
        item.source = key();
        if (!item.title.isEmpty())
            out.append(item);
    }
    return out;
}

QList<Chapter> MangaDexSource::chapters(const QString &seriesUrl) {
    const auto id = dexId(seriesUrl, QStringLiteral("title"));
    const auto wanted = languages();
    QList<Chapter> out;
    QSet<QString> seen;
    for (int offset = 0; offset < 10000; offset += 500) {
        QList<QPair<QString, QString>> params{{"offset", QString::number(offset)}, {"limit", "500"}};
        for (const auto &language : wanted)
            params.append({"translatedLanguage[]", language});
        params.append({{"order[chapter]", "asc"}, {"order[volume]", "asc"}, {"includeExternalUrl", "0"}, {"includeFuturePublishAt", "0"}});
        const auto doc = api(QStringLiteral("/manga/%1/feed").arg(id), params);
        const auto rows = arr(doc.object().value("data"));
        for (const auto &row : rows) {
            const auto c = row.toObject();
            const auto attrs = c.value("attributes").toObject();
            const auto cid = str(c.value("id"));
            if (!attrs.value("pages").toInt() || !str(attrs.value("externalUrl")).isEmpty() ||
                attrs.value("isUnavailable").toBool() || seen.contains(cid))
                continue;
            seen.insert(cid);
            Chapter ch;
            ch.number = str(attrs.value("chapter"));
            if (ch.number.isEmpty())
                ch.number = QStringLiteral("0");
            const auto title = str(attrs.value("title"));
            ch.title = chapterLabel(ch.number, title);
            ch.url = tagged(QStringLiteral("https://mangadex.org/chapter/%1").arg(cid));
            ch.slug = cid;
            ch.released = str(attrs.value("publishAt"));
            out.append(ch);
        }
        if (rows.isEmpty() || offset + rows.size() >= doc.object().value("total").toInt())
            break;
    }
    return out;
}

QList<Panel> MangaDexSource::panels(const QString &chapterUrl) {
    const auto id = dexId(chapterUrl, QStringLiteral("chapter"));
    const auto doc = api(QStringLiteral("/at-home/server/%1").arg(id), {}, true);
    const auto root = doc.object();
    const auto chapter = root.value("chapter").toObject();
    const auto hash = str(chapter.value("hash"));
    const QUrl base(str(root.value("baseUrl")));
    if (hash.isEmpty() || !(hostMatches(base.host(), QStringLiteral("mangadex.network")) ||
                            hostMatches(base.host(), QStringLiteral("mangadex.org"))))
        throw Error(QStringLiteral("This MangaDex chapter is unavailable."));
    QList<Panel> out;
    for (const auto &file : arr(chapter.value("data")))
        out.append({QStringLiteral("%1://%2/data/%3/%4")
                        .arg(base.scheme(), base.authority(), hash, QString::fromUtf8(QUrl::toPercentEncoding(str(file))))});
    return out;
}

std::optional<Details> MangaDexSource::details(const QString &seriesUrl) {
    const auto id = dexId(seriesUrl, QStringLiteral("title"));
    const auto doc = api(QStringLiteral("/manga/%1").arg(id), {{"includes[]", "cover_art"}, {"includes[]", "author"}});
    const auto manga = doc.object().value("data").toObject();
    const auto attrs = manga.value("attributes").toObject();
    const auto wanted = languages();
    Details d;
    d.title = dexTitle(attrs, wanted);
    d.description = dexText(attrs.value("description").toObject(), wanted);
    d.status = str(attrs.value("status"));
    // The same title on AniList ("al") and MyAnimeList ("mal"), when
    // MangaDex knows it; trackers use these before searching by name.
    const auto links = attrs.value("links").toObject();
    if (const auto anilist = str(links.value("al")).toInt(); anilist > 0)
        d.trackerIds.insert(QStringLiteral("anilist"), anilist);
    if (const auto mal = str(links.value("mal")).toInt(); mal > 0)
        d.trackerIds.insert(QStringLiteral("mal"), mal);
    for (const auto &tag : arr(attrs.value("tags")))
        d.genres.append(str(obj(obj(tag.toObject().value("attributes")).value("name")).value("en")));
    QStringList authors;
    for (const auto &rel : arr(manga.value("relationships"))) {
        const auto r = rel.toObject();
        const auto type = str(r.value("type"));
        if (type == QLatin1String("author"))
            authors.append(str(obj(r.value("attributes")).value("name")));
        if (type == QLatin1String("cover_art")) {
            const auto file = str(obj(r.value("attributes")).value("fileName"));
            if (!file.isEmpty())
                d.cover = QStringLiteral("https://uploads.mangadex.org/covers/%1/%2.512.jpg").arg(id, file);
        }
    }
    authors.removeAll(QString());
    d.author = authors.join(QStringLiteral(", "));
    return d;
}

// ---------------------------------------------------------------- Iken

QList<Summary> IkenSource::search(const QString &query) {
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/query"),
                                          {{"page", "1"}, {"perPage", "24"}, {"searchTerm", query}}),
                                   jsonHeaders());
    QList<Summary> out;
    for (const auto &row : arr(doc.object().value("posts"))) {
        const auto p = row.toObject();
        Summary item;
        item.title = str(p.value("postTitle"));
        item.url = QStringLiteral("%1/series/%2#%3").arg(m_config.base, str(p.value("slug")), str(p.value("id")));
        item.cover = str(p.value("featuredImage"));
        item.type = str(p.value("seriesType"));
        item.genres = names(arr(p.value("genres"))).join(QStringLiteral(", "));
        item.source = key();
        if (!item.title.isEmpty())
            out.append(item);
    }
    return out;
}

QString IkenSource::postId(const QUrl &seriesUrl) {
    static const QRegularExpression digits(QStringLiteral("^\\d+$"));
    if (digits.match(seriesUrl.fragment()).hasMatch())
        return seriesUrl.fragment();
    const auto slug = pathSegment(seriesUrl, -1);
    if (slug.isEmpty())
        return {};
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/post"), {{"postSlug", slug}}),
                                   jsonHeaders());
    return str(doc.object().value("post").toObject().value("id"));
}

QList<Chapter> IkenSource::chapters(const QString &seriesUrl) {
    const auto id = postId(QUrl(seriesUrl));
    if (id.isEmpty())
        throw Error(QStringLiteral("%1 did not recognise this series.").arg(label()));
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/chapters"),
                                          {{"postId", id}, {"skip", "0"}, {"take", "999"}, {"order", "desc"}}),
                                   jsonHeaders(), 20000);
    QList<Chapter> out;
    for (const auto &row : arr(doc.object().value("post").toObject().value("chapters"))) {
        const auto c = row.toObject();
        if (c.value("isLocked").toBool() || c.value("isPermanentlyLocked").toBool() ||
            (c.contains("isAccessible") && !c.value("isAccessible").toBool()))
            continue;
        Chapter ch;
        ch.number = str(c.value("number"));
        const auto title = str(c.value("title"));
        ch.title = chapterLabel(ch.number, title);
        ch.url = QStringLiteral("%1/api/chapter?chapterId=%2").arg(m_config.api, str(c.value("id")));
        ch.slug = str(c.value("slug"));
        if (ch.slug.isEmpty())
            ch.slug = str(c.value("id"));
        ch.released = str(c.value("createdAt"));
        if (!ch.number.isEmpty())
            out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Panel> IkenSource::panels(const QString &chapterUrl) {
    QUrl url(chapterUrl);
    if (url.path().startsWith(QLatin1String("/series/"))) {
        // A browser chapter link; find its API id through the chapter list.
        const auto seriesSlug = pathSegment(url, 1);
        const auto chapterSlug = pathSegment(url, 2);
        const auto list = chapters(QStringLiteral("%1/series/%2").arg(m_config.base, seriesSlug));
        auto it = std::find_if(list.begin(), list.end(), [&](const Chapter &c) { return c.slug == chapterSlug; });
        if (it == list.end())
            throw Error(QStringLiteral("This chapter is unavailable on %1.").arg(label()));
        url = QUrl(it->url);
    }
    const auto doc = Http::getJson(url, jsonHeaders());
    QList<std::pair<int, Panel>> rows;
    int index = 0;
    for (const auto &row : arr(doc.object().value("chapter").toObject().value("images"))) {
        const auto image = row.toObject();
        const auto order = image.contains("order") ? image.value("order").toInt() : index;
        Panel panel{str(image.value("url")), image.value("width").toInt(), image.value("height").toInt()};
        if (!panel.url.isEmpty())
            rows.append({order, panel});
        ++index;
    }
    std::stable_sort(rows.begin(), rows.end(), [](auto &a, auto &b) { return a.first < b.first; });
    QList<Panel> out;
    for (auto &[order, panel] : rows)
        out.append(panel);
    return out;
}

std::optional<Details> IkenSource::details(const QString &seriesUrl) {
    const auto slug = pathSegment(QUrl(seriesUrl), -1);
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/post"), {{"postSlug", slug}}),
                                   jsonHeaders());
    const auto p = doc.object().value("post").toObject();
    if (p.isEmpty())
        return std::nullopt;
    Details d;
    d.title = str(p.value("postTitle"));
    d.cover = str(p.value("featuredImage"));
    d.description = stripTags(str(p.value("postContent")));
    d.status = str(p.value("seriesStatus"));
    d.author = str(p.value("author"));
    d.genres = names(arr(p.value("genres")));
    return d;
}

QList<LatestItem> IkenSource::latest(int limit) {
    const int pageSize = std::clamp(limit, 12, 100);
    QList<LatestItem> out;
    for (int page = 1; out.size() < limit && page <= 5; ++page) {
        const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/query"),
                                              {{"page", QString::number(page)}, {"perPage", QString::number(pageSize)}}),
                                       jsonHeaders());
        const auto rows = arr(doc.object().value("posts"));
        for (const auto &row : rows) {
            const auto p = row.toObject();
            QJsonObject newest;
            for (const auto &c : arr(p.value("chapters")))
                if (newest.isEmpty() || c.toObject().value("number").toDouble() > newest.value("number").toDouble())
                    newest = c.toObject();
            LatestItem item;
            item.source = key();
            item.sourceLabel = label();
            item.title = str(p.value("postTitle"));
            item.url = QStringLiteral("%1/series/%2#%3").arg(m_config.base, str(p.value("slug")), str(p.value("id")));
            item.cover = str(p.value("featuredImage"));
            item.chapterNumber = str(newest.value("number"));
            item.chapterTitle = str(newest.value("title"));
            if (item.chapterTitle.isEmpty() && !item.chapterNumber.isEmpty())
                item.chapterTitle = chapterLabel(item.chapterNumber);
            if (!str(newest.value("id")).isEmpty())
                item.chapterUrl = QStringLiteral("%1/api/chapter?chapterId=%2").arg(m_config.api, str(newest.value("id")));
            for (const auto &value : {newest.value("createdAt"), p.value("lastChapterAddedAt"), p.value("updatedAt")})
                if (item.publishedAt.isEmpty())
                    item.publishedAt = str(value);
            if (!item.title.isEmpty())
                out.append(item);
        }
        if (rows.size() < pageSize)
            break;
    }
    return out;
}

// ---------------------------------------------------------------- HiveToons

QList<Summary> HiveSource::search(const QString &query) {
    const auto doc = Http::getJson(
        apiUrl(m_config.api, QStringLiteral("/api/query"),
               {{"page", "1"}, {"perPage", "40"}, {"view", "archive"}, {"searchTerm", query}}),
        jsonHeaders());
    QList<Summary> out;
    for (const auto &row : arr(doc.object().value("posts"))) {
        const auto p = row.toObject();
        Summary item;
        item.title = str(p.value("postTitle"));
        item.url = QStringLiteral("%1/series/%2").arg(m_config.base, str(p.value("slug")));
        item.cover = str(p.value("featuredImage"));
        item.type = str(p.value("seriesType"));
        item.genres = names(arr(p.value("genres"))).join(QStringLiteral(", "));
        item.source = key();
        if (!item.title.isEmpty())
            out.append(item);
    }
    return out;
}

QList<Chapter> HiveSource::chapters(const QString &seriesUrl) {
    const QUrl url(seriesUrl);
    const auto slug = pathSegment(url, 1);
    if (pathSegment(url, 0) != QLatin1String("series") || slug.isEmpty())
        throw Error(QStringLiteral("That is not a HiveToons series link."));
    // Match the complete slug so a related result cannot supply another
    // book's id.
    const auto catalog = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/query"),
                                              {{"page", "1"},
                                               {"perPage", "40"},
                                               {"view", "archive"},
                                               {"searchTerm", QString(slug).replace(QLatin1Char('-'), QLatin1Char(' '))}}),
                                       jsonHeaders());
    QString id;
    for (const auto &row : arr(catalog.object().value("posts")))
        if (str(row.toObject().value("slug")) == slug)
            id = str(row.toObject().value("id"));
    if (id.isEmpty())
        throw Error(QStringLiteral("HiveToons did not recognise this series."));
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/api/chapters"),
                                          {{"postId", id}, {"skip", "0"}, {"take", "all"}, {"order", "desc"}, {"view", "manage"}}),
                                   jsonHeaders(), 20000);
    static const QRegularExpression chapterSlug(QStringLiteral(R"(^chapter-\d+(?:\.\d+)?$)"));
    QList<Chapter> out;
    for (const auto &row : arr(doc.object().value("post").toObject().value("chapters"))) {
        const auto c = row.toObject();
        if (c.value("isAccessible").toBool() != true || c.value("isLocked").toBool() ||
            c.value("isPermanentlyLocked").toBool() || c.value("isShortLinkLocked").toBool() ||
            !chapterSlug.match(str(c.value("slug"))).hasMatch())
            continue;
        Chapter ch;
        ch.number = str(c.value("number"));
        ch.title = str(c.value("title"));
        if (ch.title.isEmpty())
            ch.title = chapterLabel(ch.number);
        ch.slug = str(c.value("slug"));
        ch.url = QStringLiteral("%1/series/%2/%3").arg(m_config.base, slug, ch.slug);
        ch.thumbnail = str(c.value("featuredImage"));
        ch.released = str(c.value("createdAt"));
        out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Panel> HiveSource::panels(const QString &chapterUrl) {
    const auto page = Http::getOk(QUrl(chapterUrl), htmlHeaders());
    HtmlDocument doc(page.body);
    QSet<QString> seen;
    QList<Panel> out;
    const auto cdn = m_config.options.value("imageHost").toString(QStringLiteral("https://storage.hivetoon.com/"));
    for (const auto &img : doc.select(QStringLiteral("img[alt*=\"Page \"][src]"))) {
        const auto url = img.attr("src").trimmed();
        if (!url.startsWith(cdn) || seen.contains(url))
            continue;
        seen.insert(url);
        out.append({url, img.attr("width").toInt(), img.attr("height").toInt()});
    }
    return out;
}

// ---------------------------------------------------------------- Qi Manga

QList<Summary> QiMangaSource::search(const QString &query) {
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/series/search"),
                                          {{"q", query}, {"page", "1"}, {"perPage", "20"}}),
                                   jsonHeaders());
    QList<Summary> out;
    for (const auto &row : arr(doc.object().value("data"))) {
        const auto s = row.toObject();
        Summary item;
        item.title = str(s.value("title"));
        const auto slug = str(s.value("slug"));
        item.url = QStringLiteral("%1/series/%2").arg(m_config.base, slug);
        item.cover = str(s.value("cover"));
        item.type = str(s.value("type"));
        item.source = key();
        if (!item.title.isEmpty() && !slug.isEmpty())
            out.append(item);
    }
    return out;
}

QList<Chapter> QiMangaSource::chapters(const QString &seriesUrl) {
    const QUrl url(seriesUrl);
    const auto parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const auto at = parts.indexOf(QStringLiteral("series"));
    const auto slug = at >= 0 && at + 1 < parts.size() ? parts.at(at + 1) : QString();
    if (slug.isEmpty())
        throw Error(QStringLiteral("That is not a Qi Manga series link."));
    QList<Chapter> out;
    int totalPages = 1;
    for (int page = 1; page <= totalPages && page <= 50; ++page) {
        const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/series/%1/chapters").arg(slug),
                                              {{"page", QString::number(page)}, {"perPage", "100"}, {"sort", "desc"}}),
                                       jsonHeaders(), 20000);
        totalPages = std::max(1, doc.object().value("totalPages").toInt(1));
        for (const auto &row : arr(doc.object().value("data"))) {
            const auto c = row.toObject();
            Chapter ch;
            ch.slug = str(c.value("slug"));
            ch.number = str(c.value("number"));
            ch.title = str(c.value("title"));
            if (ch.title.isEmpty())
                ch.title = chapterLabel(ch.number);
            ch.url = QStringLiteral("%1/series/%2/%3").arg(m_config.base, slug, ch.slug);
            ch.thumbnail = str(c.value("cover"));
            ch.released = str(c.value("createdAt"));
            if (!ch.number.isEmpty() && !ch.slug.isEmpty())
                out.append(ch);
        }
    }
    sortChapters(out);
    return out;
}

QList<Panel> QiMangaSource::panels(const QString &chapterUrl) {
    const QUrl url(chapterUrl);
    const auto parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const auto at = parts.indexOf(QStringLiteral("series"));
    if (at < 0 || at + 2 >= parts.size())
        throw Error(QStringLiteral("That is not a Qi Manga chapter link."));
    const auto doc = Http::getJson(
        apiUrl(m_config.api, QStringLiteral("/series/%1/chapters/%2").arg(parts.at(at + 1), parts.last())),
        jsonHeaders());
    QList<std::pair<int, Panel>> rows;
    int index = 0;
    for (const auto &row : arr(doc.object().value("images"))) {
        const auto image = row.toObject();
        const auto order = image.value("order").isDouble() ? image.value("order").toInt() : index;
        Panel panel{str(image.value("url")), image.value("width").toInt(), image.value("height").toInt()};
        if (!panel.url.isEmpty())
            rows.append({order, panel});
        ++index;
    }
    std::stable_sort(rows.begin(), rows.end(), [](auto &a, auto &b) { return a.first < b.first; });
    QList<Panel> out;
    for (auto &[order, panel] : rows)
        out.append(panel);
    return out;
}

std::optional<Details> QiMangaSource::details(const QString &seriesUrl) {
    const QUrl url(seriesUrl);
    const auto parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const auto at = parts.indexOf(QStringLiteral("series"));
    if (at < 0 || at + 1 >= parts.size())
        return std::nullopt;
    const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/series/%1").arg(parts.at(at + 1))),
                                   jsonHeaders());
    auto s = doc.object();
    if (s.value("data").isObject())
        s = s.value("data").toObject();
    if (s.isEmpty())
        return std::nullopt;
    Details d;
    d.title = str(s.value("title"));
    d.cover = str(s.value("cover"));
    d.description = stripTags(str(s.value("description")));
    d.status = str(s.value("status"));
    d.genres = names(arr(s.value("genres")));
    d.author = str(s.value("author"));
    return d;
}

QList<LatestItem> QiMangaSource::latest(int limit) {
    const int pageSize = std::clamp(limit, 12, 100);
    QList<LatestItem> out;
    for (int page = 1; out.size() < limit && page <= 5; ++page) {
        const auto doc = Http::getJson(apiUrl(m_config.api, QStringLiteral("/home/latest"),
                                              {{"page", QString::number(page)}, {"perPage", QString::number(pageSize)}}),
                                       jsonHeaders());
        const auto rows = arr(doc.object().value("data"));
        for (const auto &row : rows) {
            const auto p = row.toObject();
            QJsonObject newest;
            for (const auto &c : arr(p.value("chapters")))
                if (newest.isEmpty() || c.toObject().value("number").toDouble() > newest.value("number").toDouble())
                    newest = c.toObject();
            LatestItem item;
            item.source = key();
            item.sourceLabel = label();
            item.title = str(p.value("title"));
            item.url = QStringLiteral("%1/series/%2").arg(m_config.base, str(p.value("slug")));
            item.cover = str(p.value("cover"));
            item.chapterNumber = str(newest.value("number"));
            item.chapterTitle = str(newest.value("title"));
            if (item.chapterTitle.isEmpty() && !item.chapterNumber.isEmpty())
                item.chapterTitle = chapterLabel(item.chapterNumber);
            if (!str(newest.value("slug")).isEmpty())
                item.chapterUrl = QStringLiteral("%1/series/%2/%3").arg(m_config.base, str(p.value("slug")), str(newest.value("slug")));
            for (const auto &value : {newest.value("createdAt"), p.value("updatedAt")})
                if (item.publishedAt.isEmpty())
                    item.publishedAt = str(value);
            if (!item.title.isEmpty())
                out.append(item);
        }
        if (rows.size() < pageSize)
            break;
    }
    return out;
}

} // namespace sc::comics
