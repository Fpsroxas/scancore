// The library's own logic on a scratch database: moving series between
// sources.

#include "core/Database.h"
#include "core/Http.h"
#include "core/Migration.h"
#include "core/ThemeLook.h"
#include "core/UpdateRules.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace sc;

class LibraryTests : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    static void addUnits(const QString &seriesId, const QStringList &numbers) {
        int index = 0;
        for (const auto &n : numbers)
            Database::exec(QStringLiteral("INSERT INTO units(series_id, unit_key, idx, number, title, url) VALUES(?, ?, ?, ?, ?, ?)"),
                           {seriesId, seriesId + QLatin1Char('/') + n, index++, n, QStringLiteral("Chapter ") + n,
                            seriesId + QLatin1Char('/') + n});
    }
    static void read(const QString &seriesId, const QString &number, double percent, qint64 at) {
        Database::exec(QStringLiteral("INSERT INTO progress(series_id, unit_key, unit_number, percent, completed, updated_at) "
                                      "VALUES(?, ?, ?, ?, ?, ?)"),
                       {seriesId, seriesId + QLatin1Char('/') + number, number, percent, percent >= 1 ? 1 : 0, at});
    }
    static QVariantMap progress(const QString &seriesId, const QString &number) {
        return Database::one(QStringLiteral("SELECT percent, completed, updated_at FROM progress WHERE series_id = ? AND unit_number = ?"),
                             {seriesId, number});
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        Database::open(m_dir.filePath(QStringLiteral("library.db")));
    }

    void chapterValues() {
        QCOMPARE(*migration::chapterValue(QStringLiteral("12")), 12.0);
        QCOMPARE(*migration::chapterValue(QStringLiteral(" 12.5 ")), 12.5);
        QCOMPARE(*migration::chapterValue(QStringLiteral("0.003")), 0.003);
        QCOMPARE(*migration::chapterValue(QStringLiteral("Chapter 7")), 7.0);
        QVERIFY(!migration::chapterValue(QStringLiteral("Prologue")));
    }

    void moveCarriesProgressAndEntry() {
        const auto from = QStringLiteral("comic:old:1");
        const auto to = QStringLiteral("comic:new:1");
        addUnits(from, {"1", "2", "3", "4", "5"});
        Database::exec(QStringLiteral("INSERT INTO library(series_id, category, favourite, added_at, known_units, changed_at) "
                                      "VALUES(?, 'paused', 1, 100, 5, 100)"),
                       {from});
        read(from, "1", 1, 1000);
        read(from, "2", 1, 2000);
        read(from, "3", 1, 3000);
        read(from, "4", 0.4, 4000);
        // The new source splits chapter 2 and has one more.
        addUnits(to, {"1", "2", "2.5", "3", "4", "5", "6"});
        Database::exec(QStringLiteral("INSERT INTO bookmarks(series_id, unit_key, created_at) VALUES(?, ?, 1)"), {from, from + "/5"});
        Database::exec(QStringLiteral("INSERT INTO series_settings(series_id, value) VALUES(?, '{\"sort\":\"asc\"}')"), {from});

        const auto result = migration::move(from, to);
        // 1, 2, 2.5 and 3: everything up to the furthest chapter read.
        QCOMPARE(result.read, 4);
        QCOMPARE(result.nextKey, to + QStringLiteral("/4"));
        QCOMPARE(progress(to, "2.5").value("completed").toInt(), 1);
        QCOMPARE(progress(to, "3").value("updated_at").toLongLong(), 3000);
        // Chapter 4 keeps how far it got, unfinished, and stays the latest.
        QCOMPARE(progress(to, "4").value("percent").toDouble(), 0.4);
        QCOMPARE(progress(to, "4").value("completed").toInt(), 0);
        QVERIFY(progress(to, "5").isEmpty());
        // Its bookmark and settings came along.
        QVERIFY(Database::scalar(QStringLiteral("SELECT 1 FROM bookmarks WHERE series_id = ? AND unit_key = ?"), {to, to + "/5"}).isValid());
        QCOMPARE(Database::scalar(QStringLiteral("SELECT value FROM series_settings WHERE series_id = ?"), {to}).toString(),
                 QStringLiteral("{\"sort\":\"asc\"}"));

        const auto entry = Database::one(QStringLiteral("SELECT category, favourite, added_at, known_units FROM library WHERE series_id = ?"), {to});
        QCOMPARE(entry.value("category").toString(), QStringLiteral("paused"));
        QCOMPARE(entry.value("favourite").toInt(), 1);
        QCOMPARE(entry.value("added_at").toLongLong(), 100);
        QCOMPARE(entry.value("known_units").toInt(), 7);
        // The old one is gone (and remembered as removed, for sync), with
        // its history.
        QVERIFY(!Database::scalar(QStringLiteral("SELECT 1 FROM library WHERE series_id = ?"), {from}).isValid());
        QVERIFY(Database::scalar(QStringLiteral("SELECT 1 FROM library_removed WHERE series_id = ?"), {from}).isValid());
        QCOMPARE(Database::scalar(QStringLiteral("SELECT COUNT(*) FROM progress WHERE series_id = ?"), {from}).toInt(), 0);
    }

    void copyKeepsTheOld() {
        const auto from = QStringLiteral("novel:old:2");
        const auto to = QStringLiteral("novel:new:2");
        addUnits(from, {"1", "2"});
        addUnits(to, {"1", "2"});
        Database::exec(QStringLiteral("INSERT INTO library(series_id, category, added_at, changed_at) VALUES(?, 'reading', 5, 5)"), {from});
        read(from, "1", 1, 10);
        migration::Options options;
        options.keepOld = true;
        QCOMPARE(migration::move(from, to, options).read, 1);
        QVERIFY(Database::scalar(QStringLiteral("SELECT 1 FROM library WHERE series_id = ?"), {from}).isValid());
        QVERIFY(Database::scalar(QStringLiteral("SELECT 1 FROM library WHERE series_id = ?"), {to}).isValid());
        QCOMPARE(Database::scalar(QStringLiteral("SELECT COUNT(*) FROM progress WHERE series_id = ?"), {from}).toInt(), 1);
    }

    void updateRules() {
        using namespace updates;
        QVERIFY(finishedStatus(QStringLiteral("Completed")));
        QVERIFY(finishedStatus(QStringLiteral("Finalizado")));
        QVERIFY(finishedStatus(QString::fromUtf8("완결")));
        QVERIFY(!finishedStatus(QStringLiteral("Ongoing")));
        QVERIFY(!finishedStatus(QString::fromUtf8("연재중")));
        QVERIFY(!finishedStatus(QString()));

        const Rules rules;
        // Dropped series are not checked unless asked for.
        QVERIFY(shouldCheck({QStringLiteral("reading"), QStringLiteral("Ongoing"), 10, 10, true}, rules));
        QVERIFY(!shouldCheck({QStringLiteral("dropped"), QStringLiteral("Ongoing"), 10, 10, true}, rules));
        QVERIFY(!shouldCheck({QStringLiteral("reading"), QStringLiteral("Completed"), 10, 10, true}, rules));
        Rules strict;
        strict.skipUnread = true;
        strict.skipNotStarted = true;
        QVERIFY(!shouldCheck({QStringLiteral("reading"), QString(), 10, 4, true}, strict));
        QVERIFY(!shouldCheck({QStringLiteral("planned"), QString(), 10, 10, false}, strict));
        QVERIFY(shouldCheck({QString(), QString(), 10, 10, true}, strict));
    }

    // A theme file's look keeps known settings with sensible values and
    // known pages only.
    void themeLook() {
        // Built in code: a raw string literal here makes AutoMoc skip the file.
        const QJsonObject in{
            {"mode", "dark"}, {"accentName", "violet"},
            {"settings", QJsonObject{{"navStyle", "dock"}, {"roundness", "blobby"}, {"textScale", 9},
                                     {"readerRailCurrent", "#000"}, {"readerRailRead", "red"}, {"libraryCheckHours", 1},
                                     {"readerProgressWidth", 6.2}}},
            {"pages", QJsonObject{
                {"home", QJsonObject{{"order", QJsonArray{"continue", "trending", "../x", "continue"}},
                                     {"hidden", QJsonObject{{"popular", true}, {"x", false}}},
                                     {"look", QJsonObject{{"continue", QJsonObject{{"style", "grid"}, {"count", 999},
                                                                                  {"subtitles", false}, {"evil", 1}}}}}}},
                {"settings", QJsonObject{{"order", QJsonArray{"a"}}}}}}};
        const auto out = theme::sanitizeLook(in);
        QCOMPARE(out.value("mode").toString(), QStringLiteral("dark"));
        QCOMPARE(out.value("accentName").toString(), QStringLiteral("violet"));
        const auto settings = out.value("settings").toObject();
        QCOMPARE(settings.value("navStyle").toString(), QStringLiteral("dock"));
        QVERIFY(!settings.contains("roundness"));
        QCOMPARE(settings.value("textScale").toDouble(), 1.4);
        QCOMPARE(settings.value("readerRailCurrent").toString(), QStringLiteral("#000000"));
        QVERIFY(!settings.contains("readerRailRead"));
        QVERIFY(!settings.contains("libraryCheckHours"));
        QCOMPARE(settings.value("readerProgressWidth").toInt(), 6);
        const auto pages = out.value("pages").toObject();
        QCOMPARE(pages.keys(), QStringList{QStringLiteral("home")});
        const auto home = pages.value("home").toObject();
        QCOMPARE(home.value("order").toArray().size(), 2);
        QCOMPARE(home.value("hidden").toObject().keys(), QStringList{QStringLiteral("popular")});
        const auto look = home.value("look").toObject().value("continue").toObject();
        QCOMPARE(look.value("count").toInt(), 200);
        QVERIFY(!look.contains("evil"));
        QVERIFY(theme::sanitizeLook(QJsonObject{{"mode", "neon"}}).isEmpty());
    }

    void refusesNothingToMoveTo() {
        QVERIFY_THROWS_EXCEPTION(Error, migration::move(QStringLiteral("comic:a:1"), QStringLiteral("comic:a:1")));
        QVERIFY_THROWS_EXCEPTION(Error, migration::move(QStringLiteral("comic:a:1"), QStringLiteral("comic:empty:1")));
    }
};

QTEST_GUILESS_MAIN(LibraryTests)
#include "tst_library.moc"
