#include "Text.h"

#include <QHash>
#include <QList>

namespace sc {

QString normalizedTitle(const QString &text) {
    const auto decomposed = text.toLower().normalized(QString::NormalizationForm_D).toUcs4();
    QList<char32_t> out;
    out.reserve(decomposed.size());
    bool gap = false;
    for (const char32_t c : decomposed) {
        // Accents on Latin, Greek and Cyrillic letters. Other scripts' marks
        // (Japanese voicing, Indic vowel signs) change the word and stay.
        if (c >= 0x0300 && c <= 0x036F)
            continue;
        if (QChar::isLetterOrNumber(c) || QChar::isMark(c)) {
            if (gap && !out.isEmpty())
                out.append(U' ');
            gap = false;
            out.append(c);
        } else {
            gap = true;
        }
    }
    return QString::fromUcs4(out.constData(), out.size()).normalized(QString::NormalizationForm_C);
}

QString languageName(const QString &code) {
    static const QHash<QString, QString> names{
        {"en", "English"},  {"es", "Español"},   {"fr", "Français"}, {"pt", "Português"}, {"id", "Bahasa Indonesia"},
        {"it", "Italiano"}, {"de", "Deutsch"},   {"ru", "Русский"},  {"tr", "Türkçe"},    {"vi", "Tiếng Việt"},
        {"ar", "العربية"},  {"ja", "日本語"},    {"ko", "한국어"},   {"zh", "中文"},      {"th", "ไทย"},
        {"pl", "Polski"},   {"multi", "Multi"}};
    if (code.isEmpty())
        return names.value(QStringLiteral("en"));
    return names.value(code.section(QLatin1Char('-'), 0, 0).toLower(), code);
}

QString titleKey(const QString &text) { return normalizedTitle(text).remove(QLatin1Char(' ')); }

} // namespace sc
