#pragma once

#include <QString>
#include <QStringList>

namespace sc::updates {

// Which saved series the new-chapter check looks at (Settings > Library).
struct Rules {
    // Library lists checked: reading, planned, paused, completed, dropped.
    QStringList lists{QStringLiteral("reading"), QStringLiteral("planned"), QStringLiteral("paused"), QStringLiteral("completed")};
    // Series the site marks as finished.
    bool skipFinished = true;
    // Series with chapters already there to read.
    bool skipUnread = false;
    // Series never opened.
    bool skipNotStarted = false;
};

struct Entry {
    QString category;
    QString status;
    int knownUnits = 0;
    int readUnits = 0;
    bool started = false;
};

bool shouldCheck(const Entry &entry, const Rules &rules);

// A site's publication status saying the series is over ("Completed",
// "Finalizado", "완결", ...).
bool finishedStatus(const QString &status);

} // namespace sc::updates
