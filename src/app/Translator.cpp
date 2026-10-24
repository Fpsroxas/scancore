#include "Translator.h"

#include "App.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/ImageCache.h"
#include "core/Task.h"
#include "core/translation/Cloud.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace sc {

namespace {

using namespace translation;

constexpr qint64 kMonth = 30LL * 24 * 3600;
// Tall webtoon strips are read in slices this high, overlapping so a line
// cut at a slice's edge is whole in the next one.
constexpr int kSlice = 3000;
constexpr int kOverlap = 240;

QString setting(const char *key) { return App::instance()->settings().value(QLatin1String(key)).toString().trimmed(); }

bool onAndroid() {
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

#ifdef Q_OS_ANDROID
constexpr auto kMlKit = "com/mediumdev/scancore/MlKit";
#endif

// The page's lines from Android's recogniser.
QList<TextLine> mlkitLines(const QByteArray &image, const QString &language) {
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    auto bytes = env->NewByteArray(jsize(image.size()));
    env->SetByteArrayRegion(bytes, 0, jsize(image.size()), reinterpret_cast<const jbyte *>(image.constData()));
    const auto answer = QJniObject::callStaticMethod<jstring>(kMlKit, "recognize", "([BLjava/lang/String;)Ljava/lang/String;", bytes,
                                                             QJniObject::fromString(language).object<jstring>())
                            .toString();
    env->DeleteLocalRef(bytes);
    const auto json = QJsonDocument::fromJson(answer.toUtf8());
    if (json.isObject())
        throw Error(QObject::tr("Android could not read the page: %1").arg(json.object().value(QStringLiteral("error")).toString()));
    QList<TextLine> out;
    for (const auto &v : json.array()) {
        const auto o = v.toObject();
        out.append({QRectF(o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble(),
                           o.value(QStringLiteral("w")).toDouble(), o.value(QStringLiteral("h")).toDouble()),
                    o.value(QStringLiteral("text")).toString()});
    }
    return out;
#else
    Q_UNUSED(image)
    Q_UNUSED(language)
    throw Error(QObject::tr("This device has no text recogniser."));
#endif
}

QStringList mlkitTranslate(const QStringList &texts, const QString &from, const QString &to) {
#ifdef Q_OS_ANDROID
    if (from.isEmpty())
        throw Error(QObject::tr("Android's translator needs to know the language: pick it in the reader."));
    const auto answer = QJniObject::callStaticMethod<jstring>(
                            kMlKit, "translate", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                            QJniObject::fromString(QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(texts)).toJson(QJsonDocument::Compact)))
                                .object<jstring>(),
                            QJniObject::fromString(from).object<jstring>(), QJniObject::fromString(to).object<jstring>())
                            .toString();
    const auto json = QJsonDocument::fromJson(answer.toUtf8());
    if (json.isObject())
        throw Error(QObject::tr("Android could not translate: %1").arg(json.object().value(QStringLiteral("error")).toString()));
    QStringList out;
    for (const auto &v : json.array())
        out.append(v.toString());
    return out;
#else
    Q_UNUSED(texts)
    Q_UNUSED(from)
    Q_UNUSED(to)
    throw Error(QObject::tr("This device has no translator of its own."));
#endif
}

QByteArray encoded(const QImage &image) {
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG", 90);
    return out;
}

// Translates in batches the services take.
QStringList translateWith(const QString &engine, const QStringList &texts, const QString &from, const QString &to) {
    QStringList out;
    const int batch = engine == QLatin1String("chat") ? 40 : 50;
    for (int i = 0; i < texts.size(); i += batch) {
        const auto part = texts.mid(i, batch);
        if (engine == QLatin1String("deepl"))
            out += deepl(part, from, to, setting("translateDeeplKey"));
        else if (engine == QLatin1String("google"))
            out += google(part, from, to, setting("translateGoogleKey"));
        else if (engine == QLatin1String("chat"))
            out += chatModel(part, from, to, setting("translateChatUrl"), setting("translateChatKey"), setting("translateChatModel"));
        else if (engine == QLatin1String("mlkit"))
            out += mlkitTranslate(part, from, to);
        else if (engine == QLatin1String("fake"))
            for (const auto &t : part)
                out += QStringLiteral("Translated: ") + t;
        else
            throw Error(QObject::tr("Pick a way to translate in Settings → Reading → Translate."));
    }
    return out;
}

} // namespace

Translator *Translator::instance() {
    static auto *translator = [] {
        auto *created = new Translator;
        QJSEngine::setObjectOwnership(created, QJSEngine::CppOwnership);
        return created;
    }();
    return translator;
}

Translator::Translator(QObject *parent) : QObject(parent) {
    connect(App::instance(), &App::settingsChanged, this, &Translator::changed);
}

QString Translator::target() const {
    const auto own = setting("translateTarget");
    if (!own.isEmpty())
        return own;
    const auto app = QLocale().name().section(QLatin1Char('_'), 0, 0);
    return app.isEmpty() || app == QLatin1String("C") ? QStringLiteral("en") : app;
}

// Development hook: SCANCORE_TRANSLATE_FAKE=1 finds two bubbles on every
// page and "translates" by marking the text, to see the reader's overlay
// without keys.
bool fakeEngines() { return qEnvironmentVariableIsSet("SCANCORE_TRANSLATE_FAKE"); }

QString Translator::readerEngine() const {
    if (fakeEngines())
        return QStringLiteral("fake");
    const auto chosen = setting("translateReader");
    if (chosen == QLatin1String("vision"))
        return setting("translateGoogleKey").isEmpty() ? QString() : chosen;
    if (chosen == QLatin1String("mlkit"))
        return onAndroid() ? chosen : QString();
    // Automatic: the phone's own, else Vision with a Google key.
    if (onAndroid())
        return QStringLiteral("mlkit");
    return setting("translateGoogleKey").isEmpty() ? QString() : QStringLiteral("vision");
}

QString Translator::textEngine() const {
    if (fakeEngines())
        return QStringLiteral("fake");
    const auto chosen = setting("translateText");
    if (chosen == QLatin1String("deepl"))
        return setting("translateDeeplKey").isEmpty() ? QString() : chosen;
    if (chosen == QLatin1String("google"))
        return setting("translateGoogleKey").isEmpty() ? QString() : chosen;
    if (chosen == QLatin1String("chat"))
        return setting("translateChatUrl").isEmpty() || setting("translateChatModel").isEmpty() ? QString() : chosen;
    if (chosen == QLatin1String("mlkit"))
        return onAndroid() ? chosen : QString();
    // Automatic: a key the reader gave, else the phone's own.
    if (!setting("translateDeeplKey").isEmpty())
        return QStringLiteral("deepl");
    if (!setting("translateGoogleKey").isEmpty())
        return QStringLiteral("google");
    if (!setting("translateChatUrl").isEmpty() && !setting("translateChatModel").isEmpty())
        return QStringLiteral("chat");
    return onAndroid() ? QStringLiteral("mlkit") : QString();
}

bool Translator::pagesReady() const { return !readerEngine().isEmpty() && !textEngine().isEmpty(); }
bool Translator::textReady() const { return !textEngine().isEmpty(); }

QString Translator::hint() const {
    if (pagesReady())
        return {};
    if (readerEngine().isEmpty())
        return tr("To translate pages on a computer, add a Google Cloud key in Settings → Reading → Translate (Vision reads the "
                  "page's text).");
    return tr("Add a DeepL or Google key, or a model's address, in Settings → Reading → Translate.");
}

QVariantList Translator::readers() const {
    QVariantList out{QVariantMap{{"value", "auto"}, {"label", tr("Automatic")}}};
    if (onAndroid())
        out.append(QVariantMap{{"value", "mlkit"}, {"label", tr("This phone")}, {"detail", tr("On the phone, with Google Play services")}});
    out.append(QVariantMap{{"value", "vision"}, {"label", tr("Google Cloud Vision")}, {"detail", tr("Your Google Cloud key")}});
    return out;
}

QVariantList Translator::translators() const {
    QVariantList out{QVariantMap{{"value", "auto"}, {"label", tr("Automatic")}}};
    if (onAndroid())
        out.append(QVariantMap{{"value", "mlkit"}, {"label", tr("This phone")}, {"detail", tr("On the phone; a model per language")}});
    out.append(QVariantMap{{"value", "deepl"}, {"label", QStringLiteral("DeepL")}, {"detail", tr("Your DeepL key")}});
    out.append(QVariantMap{{"value", "google"}, {"label", tr("Google Translate")}, {"detail", tr("Your Google Cloud key")}});
    out.append(QVariantMap{{"value", "chat"}, {"label", tr("A language model")}, {"detail", tr("Any OpenAI-compatible service, or one on your computer")}});
    return out;
}

void Translator::clearCache() { Database::query(QStringLiteral("DELETE FROM cache WHERE key LIKE 'translate:%'")); }

Task *Translator::page(const QString &url, const QString &referer, const QString &from) {
    const auto reader = readerEngine();
    const auto translator = textEngine();
    const auto to = target();
    return Task::run([url, referer, from, reader, translator, to]() -> QVariant {
        if (reader.isEmpty() || translator.isEmpty())
            throw Error(Translator::instance()->hint());
        const auto key = QStringLiteral("translate:page:") +
                         QString::fromLatin1(QCryptographicHash::hash((url + QLatin1Char('|') + reader + QLatin1Char('|') + translator +
                                                                       QLatin1Char('|') + from + QLatin1Char('|') + to)
                                                                          .toUtf8(),
                                                                      QCryptographicHash::Sha1)
                                                 .toHex());
        if (const auto cached = Database::cacheGet(key))
            return QJsonDocument::fromJson(*cached).toVariant();

        const auto bytes = ImageCache::fetch(url, referer);
        QImage image;
        if (bytes.isEmpty() || !image.loadFromData(bytes))
            throw Error(QObject::tr("The page's picture could not be read."));

        // Text found in slices of a tall strip, then grouped into bubbles.
        QList<TextLine> lines;
        QList<TextBlock> blocks;
        const int height = image.height();
        for (int top = 0; top < height; top += kSlice - kOverlap) {
            const int bottom = std::min(height, top + kSlice);
            const bool whole = top == 0 && bottom == height;
            const auto part = whole ? bytes : encoded(image.copy(0, top, image.width(), bottom - top));
            // Lines whose middle is in this slice's own share (not the
            // overlap the next slice reads again).
            const double from0 = top == 0 ? 0 : top + kOverlap / 2.0;
            const double to0 = bottom == height ? height : bottom - kOverlap / 2.0;
            if (reader == QLatin1String("fake")) {
                if (top == 0) {
                    const double w = image.width(), line = w * 0.05;
                    lines.append({QRectF(w * 0.12, line * 2, w * 0.4, line), QStringLiteral("¿Qué demonios")});
                    lines.append({QRectF(w * 0.14, line * 3.1, w * 0.36, line), QStringLiteral("es esto?")});
                    lines.append({QRectF(w * 0.55, line * 8, w * 0.3, line * 1.2), QStringLiteral("¡Corre!")});
                }
            } else if (reader == QLatin1String("vision")) {
                for (auto b : googleVision(part, from, setting("translateGoogleKey"))) {
                    b.box.translate(0, top);
                    if (b.box.center().y() >= from0 && b.box.center().y() < to0)
                        blocks.append(b);
                }
            } else {
                for (auto l : mlkitLines(part, from)) {
                    l.box.translate(0, top);
                    if (l.box.center().y() >= from0 && l.box.center().y() < to0)
                        lines.append(l);
                }
            }
            if (bottom == height)
                break;
        }
        if (!lines.isEmpty())
            blocks = groupLines(lines, from);

        QStringList sources;
        for (const auto &b : std::as_const(blocks))
            sources.append(b.text);
        const auto translated = sources.isEmpty() ? QStringList() : translateWith(translator, sources, from, to);
        QJsonArray out;
        for (int i = 0; i < blocks.size(); ++i) {
            const auto &b = blocks[i];
            out.append(QJsonObject{{"x", b.box.x() / image.width()},
                                   {"y", b.box.y() / height},
                                   {"w", b.box.width() / image.width()},
                                   {"h", b.box.height() / height},
                                   {"text", translated.value(i)},
                                   {"source", b.text},
                                   {"vertical", b.vertical}});
        }
        const QJsonObject result{{"width", image.width()}, {"height", height}, {"blocks", out}, {"engine", reader + QLatin1Char('+') + translator}};
        Database::cachePut(key, QJsonDocument(result).toJson(QJsonDocument::Compact), kMonth);
        return result.toVariantMap();
    });
}

Task *Translator::texts(const QStringList &texts, const QString &from) {
    const auto translator = textEngine();
    const auto to = target();
    return Task::run([texts, from, translator, to]() -> QVariant {
        if (translator.isEmpty())
            throw Error(Translator::instance()->hint());
        // Each text on its own in the cache, so a chapter read again (or
        // half of it) costs nothing.
        QStringList out(texts.size());
        QStringList missing;
        QList<int> where;
        const auto keyFor = [&](const QString &text) {
            return QStringLiteral("translate:text:") +
                   QString::fromLatin1(QCryptographicHash::hash((translator + QLatin1Char('|') + from + QLatin1Char('|') + to + QLatin1Char('|') + text).toUtf8(),
                                                                QCryptographicHash::Sha1)
                                           .toHex());
        };
        for (int i = 0; i < texts.size(); ++i) {
            if (texts[i].trimmed().isEmpty())
                continue;
            if (const auto cached = Database::cacheGet(keyFor(texts[i])))
                out[i] = QString::fromUtf8(*cached);
            else {
                missing.append(texts[i]);
                where.append(i);
            }
        }
        if (!missing.isEmpty()) {
            const auto done = translateWith(translator, missing, from, to);
            for (int j = 0; j < where.size(); ++j) {
                out[where[j]] = done.value(j);
                Database::cachePut(keyFor(missing[j]), done.value(j).toUtf8(), kMonth);
            }
        }
        return out;
    });
}

} // namespace sc
