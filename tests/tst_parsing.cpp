// Offline tests for the HTML layer and parsers. Live site checks are a
// separate, opt-in run because sites change and CI must stay deterministic.

#include "core/Html.h"
#include "sources/comics/ComicSource.h"
#include "sources/local/Local.h"
#include "sources/novels/NovelSource.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace sc;

class Parsing : public QObject {
    Q_OBJECT

private slots:
    void selectorsReturnDocumentOrder() {
        HtmlDocument doc(QStringLiteral("<ul><li class=a>1</li><li class=b>2</li><li class=a>3</li></ul>"));
        const auto nodes = doc.select(QStringLiteral("li.a, li.b"));
        QCOMPARE(nodes.size(), 3);
        QCOMPARE(nodes[0].text(), QStringLiteral("1"));
        QCOMPARE(nodes[2].text(), QStringLiteral("3"));
    }

    void attributesAndClosest() {
        HtmlDocument doc(QStringLiteral("<div class=\"row\"><h3 class=\"title\"><a href=\"/n/x\" title=\"X\">X</a></h3><img data-src=\"/c.jpg\"></div>"));
        const auto a = doc.first(QStringLiteral("h3[class*='title'] > a"));
        QCOMPARE(a.attr("title"), QStringLiteral("X"));
        const auto row = a.closest(QStringLiteral(".row"));
        QVERIFY(!row.isNull());
        QCOMPARE(novels::imgSrc(row.first(QStringLiteral("img")), QStringLiteral("https://example.com/")),
                 QStringLiteral("https://example.com/c.jpg"));
    }

    void resolveRejectsScripts() {
        QVERIFY(resolveUrl(QStringLiteral("https://a.test/"), QStringLiteral("javascript:alert(1)")).isEmpty());
        QVERIFY(resolveUrl(QStringLiteral("https://a.test/"), QStringLiteral("data:image/png;base64,AA")).isEmpty());
        QCOMPARE(resolveUrl(QStringLiteral("https://a.test/x/"), QStringLiteral("../y")), QStringLiteral("https://a.test/y"));
    }

    void entities() {
        QCOMPARE(decodeEntities(QStringLiteral("Tom &amp; Jerry &#8211; Ep")), QString::fromUtf8("Tom & Jerry – Ep"));
    }

    void textBlocksKeepFormattingOnly() {
        HtmlDocument doc(QStringLiteral(
            "<div id=c><p>Hello <i>world</i> <script>x()</script></p><p style=\"display:none\">hidden</p>"
            "<h2>Title</h2>line one<br>line two<p><a href=\"https://evil.test\" onclick=\"x\">link</a> &lt;b&gt;</p></div>"));
        const auto blocks = novels::textBlocks(doc.first(QStringLiteral("#c")));
        QCOMPARE(blocks.size(), 5);
        QCOMPARE(blocks[0].text, QStringLiteral("Hello world"));
        QCOMPARE(blocks[0].html, QStringLiteral("Hello <i>world</i>"));
        QCOMPARE(blocks[1].kind, QStringLiteral("heading"));
        QCOMPARE(blocks[2].text, QStringLiteral("line one"));
        QCOMPARE(blocks[3].text, QStringLiteral("line two"));
        // Markup from the page arrives escaped, never as tags.
        QCOMPARE(blocks[4].html, QStringLiteral("link &lt;b&gt;"));
    }

    void chapterNumbers() {
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Chapter 12.5 - The end")), QStringLiteral("12.5"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Ep. 7")), QStringLiteral("7"));
        QCOMPARE(comics::chapterNumberFrom(QStringLiteral("Prologue")), QStringLiteral("0"));
        QList<comics::Chapter> list{{QStringLiteral("10")}, {QStringLiteral("2")}, {QStringLiteral("2.5")}};
        comics::sortChapters(list);
        QCOMPARE(list[0].number, QStringLiteral("2"));
        QCOMPARE(list[2].number, QStringLiteral("10"));
    }

    void gunzip() {
        // Python's gzip, with a file name in the header.
        const auto packed = QByteArray::fromHex("1f8b08080000000002ff6261636b75702e7461636869626b000b7676f473f60f7255484a4cce2e2d50081ee58ff2e9c8070008b786ca80020000");
        QCOMPARE(local::gunzip(packed), QByteArray("SCANCORE backup ").repeated(40));
        QVERIFY(local::gunzip(packed.left(12)).isEmpty());
        QVERIFY(local::gunzip("not gzip at all").isEmpty());
    }

    void manifestIsValid() {
        QFile file(QStringLiteral(SCANCORE_SOURCE_DIR "/sources/manifest.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto doc = QJsonDocument::fromJson(file.readAll()).object();
        QVERIFY(doc.value("version").toInt() >= 1);
        QSet<QString> keys;
        for (const auto &section : {QStringLiteral("comics"), QStringLiteral("novels")})
            for (const auto &value : doc.value(section).toArray()) {
                const auto o = value.toObject();
                const auto key = section + o.value("key").toString();
                QVERIFY2(!keys.contains(key), qPrintable(key));
                keys.insert(key);
                QVERIFY(o.value("base").toString().startsWith(QLatin1String("https://")));
                QVERIFY(!o.value("hosts").toArray().isEmpty());
                if (section == QLatin1String("comics")) {
                    comics::SourceConfig config;
                    config.engine = o.value("engine").toString();
                    QVERIFY2(comics::createSource(config) != nullptr, qPrintable(config.engine));
                } else {
                    novels::SourceConfig config;
                    config.engine = o.value("engine").toString();
                    QVERIFY2(novels::createSource(config) != nullptr, qPrintable(config.engine));
                }
            }
    }
};

QTEST_GUILESS_MAIN(Parsing)
#include "tst_parsing.moc"
