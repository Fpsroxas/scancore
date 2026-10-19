#pragma once

#include <QString>

namespace sc {

// Titles as search and matching compare them: lower case, accents dropped
// ("Pequeña" and "pequena" are the same), and every run of characters that
// are not letters or digits one space. Letters of any script are kept, so
// Japanese, Korean and Chinese titles stay whole.
QString normalizedTitle(const QString &text);

// The same without spaces, for exact matches ("Año 2" -> "ano2").
QString titleKey(const QString &text);

// A content language's name in that language, for source lists: "en" ->
// "English", "es" -> "Español".
QString languageName(const QString &code);

} // namespace sc
