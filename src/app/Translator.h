#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

namespace sc {

class Task;

// Translating what SCANCORE shows: the text on a comic's page (found,
// grouped into bubbles, translated, then drawn over the page by the reader)
// and a novel's paragraphs. Engines (Settings → Reading → Translate):
// reading a page's text with Android's on-device recogniser or Google Cloud
// Vision; translating with Android's on-device translator, DeepL, Google
// Translate or any OpenAI-compatible model, with the reader's own keys.
// Answers are kept for a month per page and engine.
//
// Settings: translateTarget (a language, default the app's), translateReader
// ("auto", "mlkit", "vision"), translateText ("auto", "mlkit", "deepl",
// "google", "chat"), translateDeeplKey, translateGoogleKey, translateChatUrl,
// translateChatKey, translateChatModel.
class Translator : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Pages can be translated (a way to read them and to translate).
    Q_PROPERTY(bool pagesReady READ pagesReady NOTIFY changed)
    // Text can be translated (novels).
    Q_PROPERTY(bool textReady READ textReady NOTIFY changed)
    // What is missing, for the reader ("Add a key in Settings…").
    Q_PROPERTY(QString hint READ hint NOTIFY changed)
    Q_PROPERTY(QString target READ target NOTIFY changed)

public:
    static Translator *instance();
    static Translator *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool pagesReady() const;
    bool textReady() const;
    QString hint() const;
    QString target() const;

    // The page at `url` translated from `from` ("" = the engine guesses):
    // {width, height, blocks: [{x, y, w, h (0 to 1 of the page), text,
    // source, vertical}], engine}.
    Q_INVOKABLE sc::Task *page(const QString &url, const QString &referer, const QString &from);
    // Texts translated, in order: [string].
    Q_INVOKABLE sc::Task *texts(const QStringList &texts, const QString &from);
    // [{value, label, detail}] for Settings.
    Q_INVOKABLE QVariantList readers() const;
    Q_INVOKABLE QVariantList translators() const;
    // Forgets every translated page and text.
    Q_INVOKABLE void clearCache();

    // The engines that would be used now ("" when none).
    QString readerEngine() const;
    QString textEngine() const;

signals:
    void changed();

private:
    explicit Translator(QObject *parent = nullptr);
};

} // namespace sc
