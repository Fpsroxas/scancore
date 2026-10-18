#pragma once

#include "core/Html.h"
#include "core/Http.h"

#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QVariantMap>

#include <memory>

namespace sc::novels {

struct Book {
    QString source;
    QString url;
    QString title;
    QString author;
    QString cover;
    QString description;
    QString status;
    QString rating;
    QStringList genres;
    int chapterCount = 0;
    QVariantMap toMap() const;
};

struct ChapterRef {
    QString url;
    QString title;
    QString number;
};

// One paragraph of chapter text. `html` carries only the escaped text with
// <b>, <i>, <u> and <s>; provider markup never reaches the reader.
struct Block {
    QString kind; // paragraph | heading | quote | break
    QString text;
    QString html;
};

struct Shelf {
    QString title;
    QList<Book> books;
};

struct SourceConfig {
    QString key;
    QString label;
    QString engine;
    QString base;
    QStringList hosts;
    bool enabled = true;
    bool searchable = true;
    QString disabledReason;
    QJsonObject options;
};

class Source {
public:
    explicit Source(SourceConfig config) : m_config(std::move(config)) {}
    virtual ~Source() = default;

    const SourceConfig &config() const { return m_config; }
    QString key() const { return m_config.key; }
    QString label() const { return m_config.label; }
    virtual bool owns(const QUrl &url) const;

    virtual QList<Book> search(const QString &query) = 0;
    virtual Book book(const QString &url, QList<ChapterRef> &chapters) = 0;
    virtual QList<Block> chapter(const QString &url, QString &title) = 0;
    virtual QList<Shelf> discover() { return {}; }
    virtual bool hasDiscover() const { return false; }

protected:
    Response page(const QString &url);
    QString origin() const { return m_config.base.endsWith(QLatin1Char('/')) ? m_config.base : m_config.base + QLatin1Char('/'); }

    SourceConfig m_config;
};

std::unique_ptr<Source> createSource(const SourceConfig &config);

// Converts a chapter container into reader blocks.
QList<Block> textBlocks(const HtmlNode &container);
QString imgSrc(const HtmlNode &img, const QString &base);

} // namespace sc::novels
