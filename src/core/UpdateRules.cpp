#include "UpdateRules.h"

namespace sc::updates {

bool finishedStatus(const QString &status) {
    const auto s = status.trimmed().toLower();
    if (s.isEmpty())
        return false;
    // Words sites use for a work that has ended, in the content languages.
    static const QStringList words{
        QStringLiteral("complete"),  QStringLiteral("finished"),    QStringLiteral("ended"),       QStringLiteral("concluído"),
        QStringLiteral("concluido"), QStringLiteral("completo"),    QStringLiteral("finalizado"),  QStringLiteral("terminé"),
        QStringLiteral("terminado"), QStringLiteral("tamat"),       QStringLiteral("selesai"),     QStringLiteral("abgeschlossen"),
        QStringLiteral("완결"),       QStringLiteral("完結"),         QStringLiteral("完结"),          QStringLiteral("завершен"),
        QStringLiteral("завершён"),  QStringLiteral("tamamlandı"),  QStringLiteral("hoàn thành"),  QStringLiteral("concluso"),
        QStringLiteral("จบแล้ว")};
    for (const auto &w : words)
        if (s.contains(w))
            return true;
    return false;
}

bool shouldCheck(const Entry &entry, const Rules &rules) {
    const auto category = entry.category.isEmpty() ? QStringLiteral("reading") : entry.category;
    if (!rules.lists.contains(category))
        return false;
    if (rules.skipFinished && finishedStatus(entry.status))
        return false;
    if (rules.skipNotStarted && !entry.started)
        return false;
    if (rules.skipUnread && entry.knownUnits > entry.readUnits)
        return false;
    return true;
}

} // namespace sc::updates
