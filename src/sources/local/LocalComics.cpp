#include "Local.h"
#include "LocalSources.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace sc::local {

namespace {

QString pathOf(const QString &url) {
    const QUrl parsed(url);
    return parsed.isLocalFile() ? parsed.toLocalFile() : url;
}

// What a series folder holds as chapters: archives, PDFs and folders of
// images; or the folder's own images as one chapter.
QStringList chapterPaths(const QString &series) {
    const QFileInfo info(series);
    if (!info.isDir())
        return {series};
    QStringList chapters;
    for (const auto &entry : QDir(series).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
        if (entry.isDir()) {
            if (!QDir(entry.absoluteFilePath()).entryList({QStringLiteral("*")}, QDir::Files).filter(
                     QRegularExpression(QStringLiteral(R"(\.(jpe?g|png|webp|gif|avif|bmp|jxl)$)"),
                                        QRegularExpression::CaseInsensitiveOption)).isEmpty())
                chapters.append(entry.absoluteFilePath());
        } else if (isPagesFile(entry.fileName())) {
            chapters.append(entry.absoluteFilePath());
        }
    }
    naturalSort(chapters);
    if (chapters.isEmpty() && !pages(series).isEmpty())
        chapters.append(series);
    return chapters;
}

class LocalComicSource : public comics::Source {
public:
    using comics::Source::Source;

    bool owns(const QUrl &url) const override { return url.isLocalFile(); }

    // Each folder or comic file directly inside a chosen folder is a series.
    QList<comics::Summary> search(const QString &query) override {
        const auto words = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QList<comics::Summary> out;
        for (const auto &root : folders()) {
            QStringList entries;
            for (const auto &entry : QDir(root).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
                if (entry.isDir() || isPagesFile(entry.fileName()))
                    entries.append(entry.absoluteFilePath());
            naturalSort(entries);
            for (const auto &path : entries) {
                const auto title = titleFrom(path);
                const auto lower = title.toLower();
                if (!std::all_of(words.begin(), words.end(), [&](const QString &w) { return lower.contains(w); }))
                    continue;
                comics::Summary summary;
                summary.title = title;
                summary.url = QUrl::fromLocalFile(path).toString();
                summary.cover = cover(path);
                summary.type = QFileInfo(path).isDir() ? QStringLiteral("Folder") : QFileInfo(path).suffix().toUpper();
                summary.source = key();
                out.append(summary);
            }
        }
        return out;
    }

    QList<comics::Chapter> chapters(const QString &seriesUrl) override {
        const auto series = pathOf(seriesUrl);
        if (!QFileInfo::exists(series))
            throw Error(QStringLiteral("%1 is no longer on this device.").arg(QDir::toNativeSeparators(series)), 404);
        QList<comics::Chapter> out;
        const auto paths = chapterPaths(series);
        for (int i = 0; i < paths.size(); ++i) {
            comics::Chapter chapter;
            chapter.title = paths.size() == 1 && paths[i] == series ? titleFrom(series) : titleFrom(paths[i]);
            const auto number = comics::chapterNumberFrom(chapter.title);
            // Names without a number keep their place in the folder.
            chapter.number = number == QLatin1String("0") ? QString::number(i + 1) : number;
            chapter.url = QUrl::fromLocalFile(paths[i]).toString();
            out.append(chapter);
        }
        return out;
    }

    QList<comics::Panel> panels(const QString &chapterUrl) override {
        QList<comics::Panel> out;
        for (const auto &page : pages(pathOf(chapterUrl)))
            out.append({page, 0, 0});
        return out;
    }

    std::optional<comics::Details> details(const QString &url) override {
        const auto path = pathOf(url);
        comics::Details details;
        details.title = titleFrom(path);
        details.cover = cover(path);
        details.description = QDir::toNativeSeparators(path);
        return details;
    }

    QString imageReferer() const override { return {}; }
};

} // namespace

std::unique_ptr<comics::Source> createComicSource(const comics::SourceConfig &config) {
    return std::make_unique<LocalComicSource>(config);
}

} // namespace sc::local
