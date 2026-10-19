// Live source check: scancore_probe [comics|novels] [query] [source-key] [latest]
// Searches each enabled source, opens the first result and reads its first
// chapter (its newest with "latest"). Hits real sites, so it is not part of
// ctest.
//
// scancore_probe catalogue [query] [comick|anilist|mal] checks the tracker
// catalogues: comic and novel shelves and search.

#include "core/Http.h"
#include "sources/Manifest.h"
#include "sources/catalogue/Catalogue.h"
#include "sources/comics/Registry.h"
#include "sources/novels/Registry.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>

using namespace sc;

static QTextStream out(stdout);

template <typename F>
static bool step(const QString &label, F &&fn) {
    QElapsedTimer timer;
    timer.start();
    try {
        const auto summary = fn();
        out << "  ok   " << label << " (" << timer.elapsed() << " ms) " << summary << Qt::endl;
        return true;
    } catch (const Error &e) {
        out << "  FAIL " << label << " (" << timer.elapsed() << " ms) " << e.message() << Qt::endl;
    } catch (const std::exception &e) {
        out << "  FAIL " << label << " " << e.what() << Qt::endl;
    }
    return false;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Medium Dev"));
    QCoreApplication::setApplicationName(QStringLiteral("ScanCore"));
    QFile manifest(QStringLiteral(SCANCORE_SOURCE_DIR "/sources/manifest.json"));
    manifest.open(QIODevice::ReadOnly);
    Manifest::loadData(manifest.readAll());
    comics::Registry::rebuild();
    novels::Registry::rebuild();

    const auto args = app.arguments();
    const auto kind = args.value(1, QStringLiteral("comics"));
    const auto only = args.value(3);
    const bool newest = args.value(4) == QLatin1String("latest");
    int failures = 0;

    if (kind == QLatin1String("catalogue")) {
        catalogue::setMalClientId(QStringLiteral(SCANCORE_MAL_CLIENT_ID));
        const auto query = args.value(2, QStringLiteral("solo leveling"));
        for (const auto *key : {"comick", "anilist", "mal"}) {
            auto catalogue = catalogue::byKey(QString::fromLatin1(key));
            if (!catalogue || (!only.isEmpty() && catalogue->key() != only))
                continue;
            out << catalogue->label() << Qt::endl;
            bool ok = true;
            for (const auto *media : {"comic", "novel"}) {
                const auto m = QString::fromLatin1(media);
                if (!catalogue->supports(m))
                    continue;
                ok = step(m + QStringLiteral(" shelves"), [&] {
                    QStringList parts;
                    for (const auto &shelf : catalogue->shelves(m, false))
                        parts.append(QStringLiteral("%1 %2").arg(shelf.key).arg(shelf.entries.size()));
                    return parts.join(QStringLiteral(", "));
                }) && ok;
                ok = step(m + QStringLiteral(" search"), [&] {
                    const auto entries = catalogue->search(m, query, false);
                    if (entries.isEmpty())
                        throw Error(QStringLiteral("no results"));
                    const auto names = catalogue->names(entries.first());
                    return QStringLiteral("%1 results, first: %2 (%3 names)").arg(entries.size()).arg(entries.first().title).arg(names.size());
                }) && ok;
            }
            failures += ok ? 0 : 1;
        }
    } else if (kind == QLatin1String("comics")) {
        const auto query = args.value(2, QStringLiteral("solo leveling"));
        for (const auto &source : comics::Registry::all()) {
            if (!source->enabled() || (!only.isEmpty() && source->key() != only))
                continue;
            out << source->label() << Qt::endl;
            QList<comics::Summary> results;
            QList<comics::Chapter> chapters;
            bool ok = step("search", [&] {
                results = source->search(query);
                if (results.isEmpty())
                    throw Error(QStringLiteral("no results"));
                return QStringLiteral("%1 results, first: %2 (%3)").arg(results.size()).arg(results.first().title, results.first().url);
            });
            // Links must lead back to the source that made them.
            ok = ok && step("link", [&] {
                const auto owner = comics::Registry::forUrl(results.first().url);
                if (!owner || owner->key() != source->key())
                    throw Error(QStringLiteral("opens on %1").arg(owner ? owner->key() : QStringLiteral("no source")));
                return source->language();
            });
            ok = ok && step("chapters", [&] {
                chapters = source->chapters(results.first().url);
                if (chapters.isEmpty())
                    throw Error(QStringLiteral("no chapters"));
                return QStringLiteral("%1 chapters, last: %2").arg(chapters.size()).arg(chapters.last().title);
            });
            ok = ok && step("details", [&] {
                const auto d = source->details(results.first().url);
                if (!d)
                    return QStringLiteral("none");
                QStringList ids;
                for (auto it = d->trackerIds.begin(); it != d->trackerIds.end(); ++it)
                    ids.append(it.key() + QLatin1Char('=') + it.value().toString());
                return QStringLiteral("%1 chars of description, cover %2%3")
                    .arg(d->description.size())
                    .arg(!d->cover.isEmpty())
                    .arg(ids.isEmpty() ? QString() : QStringLiteral(", ") + ids.join(QLatin1Char(' ')));
            });
            QList<comics::Panel> panels;
            ok = ok && step("panels", [&] {
                panels = source->panels(newest ? chapters.last().url : chapters.first().url);
                if (panels.isEmpty())
                    throw Error(QStringLiteral("no panels"));
                return QStringLiteral("%1 panels, first: %2").arg(panels.size()).arg(panels.first().url.left(90));
            });
            // The first page, fetched as the reader does.
            ok = ok && step("image", [&] {
                const auto image = Http::get(QUrl(panels.first().url), {{"Referer", source->imageReferer().toUtf8()}}, 20000);
                const auto &type = image.contentType;
                if (!image.ok() || !(type.startsWith("image/") || type.startsWith("application/octet-stream")))
                    throw Error(QStringLiteral("HTTP %1, %2").arg(image.status).arg(QString::fromUtf8(type)));
                return QStringLiteral("%1 KB %2").arg(image.body.size() / 1024).arg(QString::fromUtf8(type));
            });
            if (source->hasLatest())
                ok = step("latest", [&] { return QStringLiteral("%1 items").arg(source->latest(10).size()); }) && ok;
            failures += ok ? 0 : 1;
        }
    } else {
        const auto query = args.value(2, QStringLiteral("lord of the mysteries"));
        for (const auto &source : novels::Registry::all()) {
            if (!source->config().enabled || (!only.isEmpty() && source->key() != only))
                continue;
            out << source->label() << Qt::endl;
            QList<novels::Book> books;
            QList<novels::ChapterRef> chapters;
            bool ok = step("search", [&] {
                books = source->search(query);
                if (books.isEmpty())
                    throw Error(QStringLiteral("no results"));
                return QStringLiteral("%1 results, first: %2").arg(books.size()).arg(books.first().title);
            });
            ok = ok && step("book", [&] {
                const auto book = source->book(books.first().url, chapters);
                if (chapters.isEmpty())
                    throw Error(QStringLiteral("no chapters"));
                return QStringLiteral("%1, %2 chapters").arg(book.title).arg(chapters.size());
            });
            ok = ok && step("chapter", [&] {
                QString title;
                const auto blocks = source->chapter(chapters.first().url, title);
                return QStringLiteral("\"%1\", %2 paragraphs, starts: %3").arg(title).arg(blocks.size()).arg(blocks.first().text.left(60));
            });
            if (source->hasDiscover())
                ok = step("discover", [&] {
                    const auto shelves = source->discover();
                    return QStringLiteral("%1 shelves").arg(shelves.size());
                }) && ok;
            failures += ok ? 0 : 1;
        }
    }
    out << failures << " source(s) with failures" << Qt::endl;
    return failures ? 1 : 0;
}
