// Spanish sites with an engine of their own: MangaSin.
#include "Engines.h"
#include "core/Aes.h"
#include "core/Html.h"

#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>

namespace sc::comics {

namespace {

// An image's address: lazy-loading attributes first, never a data: URL.
QString imageOf(const HtmlNode &img, const QString &base) {
    for (const char *name : {"data-src", "data-lazy-src", "src"}) {
        const auto value = img.attr(name).trimmed();
        if (!value.isEmpty() && !value.startsWith(QLatin1String("data:")) && !value.endsWith(QLatin1String("loading.gif")))
            return resolveUrl(base + QLatin1Char('/'), value);
    }
    return {};
}

} // namespace

// ---------------------------------------------------------------- MangaSin

QList<Summary> MangaSinSource::parseSearch(const QByteArray &json, const QString &base) {
    QList<Summary> out;
    QSet<QString> seen;
    for (const auto &value : QJsonDocument::fromJson(json).array()) {
        const auto o = value.toObject();
        HtmlDocument label(o.value(QStringLiteral("label")).toString());
        auto url = label.first(QStringLiteral("a[href]")).attr("href");
        if (url.isEmpty() && !str(o.value(QStringLiteral("data"))).isEmpty())
            url = base + QStringLiteral("/manga/") + str(o.value(QStringLiteral("data")));
        url = resolveUrl(base + QLatin1Char('/'), url);
        const auto title = o.value(QStringLiteral("value")).toString().trimmed();
        if (url.isEmpty() || title.isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        Summary item;
        item.title = title;
        item.url = url;
        // The list's thumbnail; the page has a bigger one.
        item.cover = label.first(QStringLiteral("img")).attr("src").replace(QLatin1String("cover_thumb."), QLatin1String("cover_250x350."));
        out.append(item);
    }
    return out;
}

std::optional<QList<Chapter>> MangaSinSource::parseChapters(const QByteArray &html, const QString &seriesUrl, const QByteArray &key) {
    // {\"ct\":\"…\",\"iv\":\"…\",\"s\":\"…\"} inside a script's string.
    static const QRegularExpression data(QStringLiteral(R"re(\{(?=[^}]*\\"ct\\")(?=[^}]*\\"s\\")[^}]*\})re"));
    const auto match = data.match(QString::fromUtf8(html));
    if (!match.hasMatch())
        throw Error(QObject::tr("MangaSin changed its series page: no chapter list found."));
    static const QRegularExpression escape(QStringLiteral(R"(\\(.))"));
    auto unescaped = match.captured(0);
    unescaped.replace(escape, QStringLiteral("\\1"));
    const auto box = QJsonDocument::fromJson(unescaped.toUtf8()).object();
    const auto plain = cryptoJsDecrypt(key, QByteArray::fromHex(box.value(QStringLiteral("s")).toString().toLatin1()),
                                       QByteArray::fromBase64(box.value(QStringLiteral("ct")).toString().toLatin1()));
    if (!plain)
        return std::nullopt;
    // A JSON string holding the JSON list: read the string first.
    auto listText = QJsonDocument::fromJson("[" + *plain + "]").array().first().toString();
    if (listText.isEmpty())
        listText = QString::fromUtf8(*plain);
    QList<Chapter> out;
    const auto root = seriesUrl.endsWith(QLatin1Char('/')) ? seriesUrl.chopped(1) : seriesUrl;
    for (const auto &value : QJsonDocument::fromJson(listText.toUtf8()).array()) {
        const auto c = value.toObject();
        const auto slug = str(c.value(QStringLiteral("slug")));
        if (slug.isEmpty())
            continue;
        Chapter ch;
        ch.number = str(c.value(QStringLiteral("number")));
        const auto name = str(c.value(QStringLiteral("name")));
        const auto plainName = QStringLiteral("Capítulo ") + ch.number;
        ch.title = name.isEmpty() || name == plainName ? plainName : plainName + QStringLiteral(": ") + name;
        ch.url = root + QLatin1Char('/') + slug;
        ch.slug = slug;
        ch.released = str(c.value(QStringLiteral("created_at")));
        out.append(ch);
    }
    sortChapters(out);
    return out;
}

QList<Panel> MangaSinSource::parsePanels(const QByteArray &html, const QString &base) {
    HtmlDocument doc(html);
    QList<Panel> out;
    QSet<QString> seen;
    for (const auto &img : doc.select(QStringLiteral("#all img.img-responsive"))) {
        auto url = imageOf(img, base);
        // Some addresses come base64-wrapped ("https://<base64>").
        if (!url.isEmpty() && !QUrl(url).isValid()) {
            const auto inner = QByteArray::fromBase64(url.section(QStringLiteral("://"), 1).toLatin1());
            url = QUrl::fromPercentEncoding(inner);
        }
        if (url.isEmpty() || seen.contains(url))
            continue;
        seen.insert(url);
        out.append({url});
    }
    return out;
}

QByteArray MangaSinSource::keyFromScript(const QByteArray &script) {
    // Everything a page script may touch answers to anything and does
    // nothing; CryptoJS only notes the passphrase.
    static const auto prelude = QStringLiteral(R"js(
        var __keys = [];
        var __any = new Proxy(function () {}, {
            get: function (t, k) { return k === Symbol.toPrimitive ? function () { return ""; } : __any; },
            apply: function () { return __any; },
            construct: function () { return __any; }
        });
        var window = this, self = this, document = __any, navigator = { userAgent: "Mozilla/5.0" };
        var location = { href: "https://m440.in/manga/x", hostname: "m440.in", pathname: "/manga/x" };
        var $ = __any, jQuery = __any, dayjs = __any, moment = __any, console = { log: function () {}, warn: function () {}, error: function () {} };
        var setTimeout = function (f) { try { if (typeof f === "function") f(); } catch (e) {} }, setInterval = function () {}, clearTimeout = function () {};
        var UsaPoncho = "{\"ct\":\"x\",\"iv\":\"y\",\"s\":\"z\"}";
        var CryptoJS = {
            AES: { decrypt: function (data, key) { __keys.push(String(key)); return { toString: function () { return "[]"; } }; } },
            enc: { Utf8: {}, Hex: { parse: function () { return {}; } }, Base64: { parse: function () { return {}; } } },
            lib: { CipherParams: { create: function (o) { return o; } } }
        };
    )js");
    QJSEngine engine;
    engine.evaluate(prelude);
    // A script that never ends is stopped.
    std::mutex mutex;
    std::condition_variable done;
    bool finished = false;
    std::thread watchdog([&] {
        std::unique_lock lock(mutex);
        if (!done.wait_for(lock, std::chrono::seconds(4), [&] { return finished; }))
            engine.setInterrupted(true);
    });
    engine.evaluate(QStringLiteral("try { (function () {\n") + QString::fromUtf8(script) + QStringLiteral("\n}).call(this); } catch (e) {}"));
    {
        std::lock_guard lock(mutex);
        finished = true;
    }
    done.notify_all();
    watchdog.join();
    const auto keys = engine.globalObject().property(QStringLiteral("__keys"));
    for (int i = 0; i < keys.property(QStringLiteral("length")).toInt(); ++i) {
        const auto k = keys.property(quint32(i)).toString();
        if (k.size() >= 6)
            return k.toUtf8();
    }
    return {};
}

namespace {
QMutex g_keyMutex;
QByteArray g_key;
} // namespace

QByteArray MangaSinSource::currentKey(bool fresh) {
    QMutexLocker lock(&g_keyMutex);
    if (g_key.isEmpty())
        g_key = m_config.options.value(QStringLiteral("key")).toString().toUtf8();
    if (!fresh && !g_key.isEmpty())
        return g_key;
    auto scripts = m_config.options.value(QStringLiteral("keyScripts")).toArray();
    if (scripts.isEmpty())
        scripts = {QStringLiteral("/js/exis.js"), QStringLiteral("/js/ads2.js")};
    for (const auto &path : std::as_const(scripts)) {
        const auto response = Http::get(QUrl(m_config.base + path.toString()), htmlHeaders(), 20000);
        if (!response.ok())
            continue;
        const auto found = keyFromScript(response.body);
        if (!found.isEmpty() && found != g_key) {
            g_key = found;
            return g_key;
        }
    }
    return g_key;
}

QList<Summary> MangaSinSource::search(const QString &query) {
    auto out = parseSearch(Http::getOk(withQuery(QUrl(m_config.base + QStringLiteral("/search")), {{"q", query}}), jsonHeaders(), 20000).body,
                           m_config.base);
    for (auto &item : out)
        item.source = key();
    return out;
}

QList<Chapter> MangaSinSource::chapters(const QString &seriesUrl) {
    const auto html = Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body;
    if (auto list = parseChapters(html, seriesUrl, currentKey(false)))
        return *list;
    // The site changed its key: read it from its script again.
    if (auto list = parseChapters(html, seriesUrl, currentKey(true)))
        return *list;
    throw Error(QObject::tr("MangaSin's chapter list could not be opened (its key changed)."));
}

QList<Panel> MangaSinSource::panels(const QString &chapterUrl) {
    return parsePanels(Http::getOk(QUrl(chapterUrl), htmlHeaders(), 20000).body, m_config.base);
}

std::optional<Details> MangaSinSource::details(const QString &seriesUrl) {
    HtmlDocument doc(Http::getOk(QUrl(seriesUrl), htmlHeaders(), 20000).body);
    Details d;
    d.title = doc.first(QStringLiteral("h2.widget-title, #title")).textTrim();
    for (const auto &img : doc.select(QStringLiteral("img.img-responsive")))
        if (img.attr("src").contains(QLatin1String("/cover/"))) {
            d.cover = imageOf(img, m_config.base);
            break;
        }
    QStringList paragraphs;
    for (const auto &p : doc.select(QStringLiteral(".well p")))
        if (const auto text = p.textTrim(); !text.isEmpty() && !paragraphs.contains(text))
            paragraphs.append(text);
    d.description = paragraphs.join(QStringLiteral("\n\n"));
    const auto dts = doc.select(QStringLiteral("dl.dl-horizontal dt"));
    const auto dds = doc.select(QStringLiteral("dl.dl-horizontal dd"));
    for (int i = 0; i < std::min(dts.size(), dds.size()); ++i) {
        const auto label = dts[i].textTrim();
        if (label.startsWith(QLatin1String("Autor")))
            d.author = dds[i].textTrim();
        else if (label.startsWith(QLatin1String("Categor")) || label.startsWith(QLatin1String("Géneros")))
            for (const auto &a : dds[i].select(QStringLiteral("a")))
                if (const auto text = a.textTrim(); !text.isEmpty())
                    d.genres.append(text);
    }
    const auto state = doc.first(QStringLiteral("div.manga-name span.label")).textTrim().toLower();
    d.status = state == QLatin1String("activo") ? QStringLiteral("En curso")
             : state.contains(QLatin1String("final")) || state.contains(QLatin1String("complet")) ? QStringLiteral("Finalizado")
             : state;
    return d;
}

} // namespace sc::comics
