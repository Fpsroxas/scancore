#include "core/Text.h"

#include <QTest>

using namespace sc;

// Text helpers that need no network or database.
class TextTests : public QObject {
    Q_OBJECT

    static QStringList split(const QString &text) {
        QStringList out;
        for (const auto &[start, length] : sentenceSpans(text))
            out.append(text.mid(start, length));
        return out;
    }

private slots:
    // Reading aloud speaks a sentence at a time.
    void sentences() {
        QCOMPARE(split(QStringLiteral("He ran. She stayed! Why?  ")),
                 (QStringList{QStringLiteral("He ran."), QStringLiteral("She stayed!"), QStringLiteral("Why?")}));
        // Quotes stay with their sentence; a lone ellipsis is not one.
        QCOMPARE(split(QStringLiteral("“Wait.” He turned. …")),
                 (QStringList{QStringLiteral("“Wait.”"), QStringLiteral("He turned.")}));
        // Chinese and Japanese full stops.
        QCOMPARE(split(QStringLiteral("他走了。她留下！为什么？")),
                 (QStringList{QStringLiteral("他走了。"), QStringLiteral("她留下！"), QStringLiteral("为什么？")}));
        QCOMPARE(split(QString()).size(), 0);
        QCOMPARE(split(QStringLiteral("***")).size(), 0);
        // Positions point into the original text.
        const auto spans = sentenceSpans(QStringLiteral("  One. Two."));
        QCOMPARE(spans.size(), 2);
        QCOMPARE(spans[0].first, 2);
        QCOMPARE(spans[1].first, 7);
        QCOMPARE(spans[1].second, 4);
    }
};

QTEST_GUILESS_MAIN(TextTests)
#include "tst_text.moc"
