#pragma once

#include "core/Parallel.h"
#include "core/Text.h"

#include <QSet>
#include <QStringList>
#include <QVariantList>

#include <functional>
#include <memory>

namespace sc {

// "Read from" for a catalogue title: the series on the given sources listed
// under any of its names. The main name is asked everywhere first; other
// names only of sources that missed (three rounds at most). `readable`
// drops matches whose chapters were all taken down. Matches in the content
// language come first, then the sources' own order; each says whether it is
// in that language ("preferred"), so the caller can open the only one.
template <typename SourcePtr>
QVariantList resolveAcross(const QList<SourcePtr> &sources, const QStringList &titles,
                           const std::function<bool(const SourcePtr &, const QString &url)> &readable = {}) {
    QStringList names;
    QSet<QString> wanted;
    for (const auto &title : titles) {
        const auto n = normalizedTitle(title);
        if (n.isEmpty() || wanted.contains(n))
            continue;
        wanted.insert(n);
        names.append(title);
    }
    struct Found {
        SourcePtr source;
        QString url;
        QString title;
        QString cover;
        int order;
    };
    QList<Found> found;
    QSet<QString> done;
    for (int round = 0; round < std::min<int>(names.size(), 3) && done.size() < sources.size(); ++round) {
        QList<SourcePtr> asked;
        using Hit = QList<QStringList>;
        QList<std::function<Hit()>> jobs;
        for (const auto &source : sources)
            if (!done.contains(source->key())) {
                asked.append(source);
                jobs.append([source, name = names[round]] {
                    Hit hits;
                    for (const auto &item : source->search(name))
                        hits.append({item.title, item.url, item.cover});
                    return hits;
                });
            }
        // A source that hangs is left out of this round, not waited for.
        parallelEach<Hit>(jobs, 15000, [&](int i, const Outcome<Hit> &result) {
            if (!result.ok())
                return;
            for (const auto &hit : *result.value)
                if (wanted.contains(normalizedTitle(hit[0])) && !done.contains(asked[i]->key())) {
                    done.insert(asked[i]->key());
                    found.append({asked[i], hit[1], hit[0], hit[2], int(sources.indexOf(asked[i]))});
                }
        });
    }
    // Taken-down series are left out (one request per match, in parallel).
    if (readable && !found.isEmpty()) {
        QList<std::function<bool()>> checks;
        for (const auto &f : found)
            checks.append([readable, f] { return readable(f.source, f.url); });
        const auto results = parallel(checks);
        QList<Found> kept;
        for (int i = 0; i < found.size(); ++i)
            if (!results[i].ok() || *results[i].value)
                kept.append(found[i]);
        found = kept;
    }
    const auto language = contentLanguage();
    const auto preferred = [&](const Found &f) { return baseLanguage(f.source->config().language) == language; };
    std::stable_sort(found.begin(), found.end(), [&](const Found &a, const Found &b) {
        if (preferred(a) != preferred(b))
            return preferred(a);
        return a.order < b.order;
    });
    QVariantList matches;
    for (const auto &f : found)
        matches.append(QVariantMap{{"source", f.source->key()},
                                   {"sourceLabel", f.source->label()},
                                   {"language", f.source->config().language},
                                   {"preferred", preferred(f)},
                                   {"url", f.url},
                                   {"title", f.title},
                                   {"cover", f.cover}});
    return matches;
}

} // namespace sc
