// Offline tests for the tracker catalogues (Comick, AniList, MyAnimeList),
// the content language, "Read from" matching and MangaDex takedowns.

#include "app/Resolve.h"
#include "core/Database.h"
#include "core/Text.h"
#include "sources/catalogue/Catalogue.h"
#include "sources/bundled/comics/Engines.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

using namespace sc;

static QByteArray fixture(const char *name) {
    QFile file(QStringLiteral(SCANCORE_SOURCE_DIR "/tests/fixtures/") + QLatin1String(name));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

namespace {

// A comic source that answers every search with the same titles after a
// pause, like a site on the network.
class FakeSource : public comics::Source {
public:
    FakeSource(const QString &key, const QString &language, QStringList titles, bool readable = true, int delayMs = 0)
        : Source(config(key, language)), m_titles(std::move(titles)), m_readable(readable), m_delay(delayMs) {}

    QList<comics::Summary> search(const QString &) override {
        if (m_delay)
            QThread::msleep(m_delay);
        QList<comics::Summary> out;
        for (const auto &title : m_titles)
            out.append({title, QStringLiteral("https://%1.test/series/%2").arg(key(), titleKey(title)), {}, {}, {}, {}, {}, key()});
        return out;
    }
    QList<comics::Chapter> chapters(const QString &) override { return {}; }
    QList<comics::Panel> panels(const QString &) override { return {}; }
    bool readable(const QString &) override { return m_readable; }

private:
    static comics::SourceConfig config(const QString &key, const QString &language) {
        comics::SourceConfig c;
        c.key = key;
        c.label = key.toUpper();
        c.base = QStringLiteral("https://%1.test").arg(key);
        c.language = language;
        return c;
    }
    QStringList m_titles;
    bool m_readable;
    int m_delay;
};

using SourcePtr = std::shared_ptr<comics::Source>;

QVariantList resolve(const QList<SourcePtr> &sources, const QStringList &names) {
    return resolveAcross<SourcePtr>(sources, names, [](const SourcePtr &s, const QString &url) { return s->readable(url); });
}

} // namespace

class CatalogueTests : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        Database::open(m_dir.filePath(QStringLiteral("test.db")));
    }

    void contentLanguageOrder() {
        // The source-language setting wins, then the interface language.
        QCOMPARE(contentLanguage(QStringLiteral("es"), QStringLiteral("fr"), QStringLiteral("de_DE")), QStringLiteral("es"));
        QCOMPARE(contentLanguage(QString(), QStringLiteral("pt_BR"), QStringLiteral("de_DE")), QStringLiteral("pt"));
        // "system" is the system locale.
        QCOMPARE(contentLanguage(QString(), QStringLiteral("system"), QStringLiteral("id_ID")), QStringLiteral("id"));
        // Otherwise English.
        QCOMPARE(contentLanguage(QString(), QStringLiteral("system"), QStringLiteral("C")), QStringLiteral("en"));
        QCOMPARE(baseLanguage(QStringLiteral("es-la")), QStringLiteral("es"));
        QCOMPARE(baseLanguage(QStringLiteral("es_419")), QStringLiteral("es"));
    }

    void comickShelves() {
        const auto top = QJsonDocument::fromJson(fixture("catalogue-comick-top.json")).object();
        const auto shelves = catalogue::comickShelves(top, false);
        QCOMPARE(shelves.size(), 4);
        QCOMPARE(shelves[0].key, QStringLiteral("trending"));
        // The week's feed, without erotica.
        QCOMPARE(shelves[0].entries.size(), 1);
        const auto &first = shelves[0].entries[0];
        QCOMPARE(first.catalog, QStringLiteral("comick"));
        QCOMPARE(first.id, QStringLiteral("aa11"));
        QCOMPARE(first.type, QStringLiteral("Manhwa"));
        QCOMPARE(first.status, QStringLiteral("Ongoing"));
        QCOMPARE(first.lastChapter, QStringLiteral("120"));
        QCOMPARE(first.titles, (QStringList{QStringLiteral("First Title"), QString::fromUtf8("Primer Título")}));
        QCOMPARE(first.cover, QStringLiteral("https://meo.comick.pictures/cover-one-m.jpg"));
        QCOMPARE(first.ids.value("comick").toString(), QStringLiteral("aa11"));
        QCOMPARE(catalogue::comickShelves(top, true)[0].entries.size(), 2);
        QCOMPARE(shelves[1].entries[0].lastChapter, QStringLiteral("12.5"));
        QCOMPARE(shelves[1].entries[0].status, QStringLiteral("Hiatus"));
        QVERIFY(shelves[2].entries.isEmpty());
    }

    void anilistEntries() {
        const auto media = QJsonDocument::fromJson(fixture("catalogue-anilist.json")).object().value("media").toArray();
        const auto comic = catalogue::anilistEntry(media[0].toObject());
        QCOMPARE(comic.media, QStringLiteral("comic"));
        QCOMPARE(comic.title, QStringLiteral("Test English"));
        QCOMPARE(comic.titles.size(), 4);
        QCOMPARE(comic.type, QStringLiteral("Manhwa"));
        QCOMPARE(comic.rating, QStringLiteral("7.9"));
        QCOMPARE(comic.ids.value("anilist").toInt(), 1001);
        QCOMPARE(comic.ids.value("mal").toInt(), 2001);
        const auto novel = catalogue::anilistEntry(media[1].toObject());
        QCOMPARE(novel.media, QStringLiteral("novel"));
        QCOMPARE(novel.type, QStringLiteral("Novel"));
        QCOMPARE(novel.title, QStringLiteral("Shousetsu"));
        QCOMPARE(novel.lastChapter, QStringLiteral("300"));
        QVERIFY(novel.adult);
        QVERIFY(!novel.ids.contains("mal"));
        QVERIFY(novel.rating.isEmpty());
    }

    void malEntries() {
        const auto rows = QJsonDocument::fromJson(fixture("catalogue-mal.json")).object().value("data").toArray();
        const auto comic = catalogue::malEntry(rows[0].toObject().value("node").toObject());
        QCOMPARE(comic.media, QStringLiteral("comic"));
        QCOMPARE(comic.type, QStringLiteral("Manhwa"));
        QCOMPARE(comic.cover, QStringLiteral("https://img.test/m/3001l.jpg"));
        QCOMPARE(comic.year, 2021);
        QCOMPARE(comic.rating, QStringLiteral("9.1"));
        QCOMPARE(comic.titles.size(), 4);
        QVERIFY(!comic.adult);
        const auto novel = catalogue::malEntry(rows[1].toObject().value("node").toObject());
        QCOMPARE(novel.media, QStringLiteral("novel"));
        QCOMPARE(novel.type, QStringLiteral("Light novel"));
        QCOMPARE(novel.status, QStringLiteral("Ongoing"));
        QVERIFY(novel.lastChapter.isEmpty());
        QVERIFY(novel.adult);
    }

    void entryRoundTrip() {
        const auto media = QJsonDocument::fromJson(fixture("catalogue-anilist.json")).object().value("media").toArray();
        const auto entry = catalogue::anilistEntry(media[0].toObject());
        const auto back = catalogue::Entry::fromMap(entry.toMap());
        QCOMPARE(back.catalog, entry.catalog);
        QCOMPARE(back.id, entry.id);
        QCOMPARE(back.titles, entry.titles);
        QCOMPARE(back.ids, entry.ids);
        // Comick entries saved before 2.0 carry only "hid".
        const auto old = catalogue::Entry::fromMap({{"catalog", "comick"}, {"hid", "xy"}, {"title", "T"}});
        QCOMPARE(old.id, QStringLiteral("xy"));
        QCOMPARE(old.titles, QStringList{QStringLiteral("T")});
    }

    void catalogueChoice() {
        catalogue::setMalClientId(QString());
        QVERIFY(!catalogue::forMedia(QStringLiteral("none"), QStringLiteral("comic")));
        QCOMPARE(catalogue::forMedia(QStringLiteral("comick"), QStringLiteral("comic"))->key(), QStringLiteral("comick"));
        // Comick has no novels: AniList's catalogue stands in.
        QCOMPARE(catalogue::forMedia(QStringLiteral("comick"), QStringLiteral("novel"))->key(), QStringLiteral("anilist"));
        // MyAnimeList needs the client id.
        QVERIFY(!catalogue::byKey(QStringLiteral("mal")));
        catalogue::setMalClientId(QStringLiteral("test-id"));
        QCOMPARE(catalogue::forMedia(QStringLiteral("mal"), QStringLiteral("novel"))->key(), QStringLiteral("mal"));
        catalogue::setMalClientId(QString());
        // Before 2.0 the catalogue was switched by comickCatalog.
        QCOMPARE(catalogue::chosen(QStringLiteral("comic")), QStringLiteral("comick"));
        Database::setPref(QStringLiteral("setting.comickCatalog"), false);
        QCOMPARE(catalogue::chosen(QStringLiteral("comic")), QStringLiteral("none"));
        // Each media has its own choice.
        Database::setPref(QStringLiteral("setting.catalogComic"), QStringLiteral("mal"));
        QCOMPARE(catalogue::chosen(QStringLiteral("comic")), QStringLiteral("mal"));
        QCOMPARE(catalogue::chosen(QStringLiteral("novel")), QStringLiteral("anilist"));
        Database::setPref(QStringLiteral("setting.catalogNovel"), QStringLiteral("none"));
        QCOMPARE(catalogue::chosen(QStringLiteral("novel")), QStringLiteral("none"));
    }

    void readFromPrefersContentLanguage() {
        Database::setPref(QStringLiteral("setting.sourceLanguages"), QStringLiteral("es"));
        const QList<SourcePtr> sources{
            std::make_shared<FakeSource>(QStringLiteral("one"), QStringLiteral("en"), QStringList{QStringLiteral("The Title")}),
            std::make_shared<FakeSource>(QStringLiteral("two"), QStringLiteral("es"), QStringList{QString::fromUtf8("El Título")}),
            std::make_shared<FakeSource>(QStringLiteral("three"), QStringLiteral("es-la"), QStringList{QStringLiteral("Other")}),
            // Listed but taken down: left out.
            std::make_shared<FakeSource>(QStringLiteral("four"), QStringLiteral("es"), QStringList{QStringLiteral("The Title")}, false)};
        const auto matches = resolve(sources, {QStringLiteral("The Title"), QStringLiteral("El Titulo")});
        QCOMPARE(matches.size(), 2);
        // Spanish first, though its source comes second; found by the
        // accent-free second name.
        QCOMPARE(matches[0].toMap().value("source").toString(), QStringLiteral("two"));
        QVERIFY(matches[0].toMap().value("preferred").toBool());
        QCOMPARE(matches[1].toMap().value("source").toString(), QStringLiteral("one"));
        QVERIFY(!matches[1].toMap().value("preferred").toBool());
        Database::setPref(QStringLiteral("setting.sourceLanguages"), QString());
    }

    // Sources are asked at the same time, so twenty slow sources take about
    // as long as one.
    void readFromFansOut() {
        QList<SourcePtr> sources;
        for (int i = 0; i < 20; ++i)
            sources.append(std::make_shared<FakeSource>(QStringLiteral("s%1").arg(i), QStringLiteral("en"),
                                                        QStringList{QStringLiteral("Slow")}, true, 150));
        QElapsedTimer timer;
        timer.start();
        const auto matches = resolve(sources, {QStringLiteral("Slow")});
        QCOMPARE(matches.size(), 20);
        QVERIFY2(timer.elapsed() < 1200, qPrintable(QStringLiteral("took %1 ms").arg(timer.elapsed())));
    }

    void mangadexTakedowns() {
        comics::SourceConfig config;
        config.key = QStringLiteral("mangadex");
        config.base = QStringLiteral("https://mangadex.org");
        comics::MangaDexSource source(config);
        const auto rows = QJsonDocument::fromJson(fixture("mangadex-feed-taken-down.json")).object().value("data").toArray();
        QSet<QString> seen;
        int takenDown = 0;
        const auto chapters = source.feedChapters(rows, seen, &takenDown);
        QCOMPARE(takenDown, 2);
        QCOMPARE(chapters.size(), 1);
        QCOMPARE(chapters[0].number, QStringLiteral("3"));
        QCOMPARE(chapters[0].title, QStringLiteral("Chapter 3: Fin"));
        // A feed of nothing but taken-down chapters reads as none.
        QJsonArray only{rows[0], rows[1]};
        seen.clear();
        takenDown = 0;
        QVERIFY(source.feedChapters(only, seen, &takenDown).isEmpty());
        QCOMPARE(takenDown, 2);
    }
};

QTEST_GUILESS_MAIN(CatalogueTests)
#include "tst_catalogue.moc"
