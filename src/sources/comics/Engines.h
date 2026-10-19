#pragma once

#include "ComicSource.h"

namespace sc::comics {

class AsuraSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QList<LatestItem> latest(int limit) override;
    bool hasLatest() const override { return true; }
};

class WebtoonSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QString imageReferer() const override { return QStringLiteral("https://www.webtoons.com/"); }
};

class MangaDexSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

private:
    // Translations to list (option "languages", default English).
    QStringList languages() const;
    QJsonDocument api(const QString &path, const QList<QPair<QString, QString>> &params, bool atHome = false);
};

class FlameSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
};

// Iken-based sites (Vortex) expose a JSON API next to the site.
class IkenSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QList<LatestItem> latest(int limit) override;
    bool hasLatest() const override { return true; }

protected:
    QString postId(const QUrl &seriesUrl);
};

class HiveSource : public IkenSource {
public:
    using IkenSource::IkenSource;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    QList<LatestItem> latest(int) override { return {}; }
    bool hasLatest() const override { return false; }
};

class QiMangaSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;
    QList<LatestItem> latest(int limit) override;
    bool hasLatest() const override { return true; }
};

// WordPress manga themes (MangaReader/MangaStream).
class WpSource : public Source {
public:
    using Source::Source;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
    QList<Panel> panels(const QString &chapterUrl) override;
    std::optional<Details> details(const QString &seriesUrl) override;

protected:
    QList<Summary> parseSearch(const QByteArray &html) const;
    QList<Summary> searchHtml(const QString &query);
    QList<Chapter> parseChapterList(const QByteArray &html, const QString &pageUrl) const;
};

// The Madara WordPress theme.
class MadaraSource : public WpSource {
public:
    using WpSource::WpSource;
    QList<Summary> search(const QString &query) override;
    QList<Chapter> chapters(const QString &seriesUrl) override;
};

} // namespace sc::comics
