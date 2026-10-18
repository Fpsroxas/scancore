# Translations

SCANCORE's interface in other languages: one Qt Linguist file per language
(`scancore_<language>.ts`), built into the app. Settings → Interface →
Language switches between them while the app runs.

## Improving a translation

Open the `.ts` file in Qt Linguist (or any text editor: each `<message>`
has the English `<source>` and its `<translation>`) and open a pull
request. Some texts are pieces of a sentence that the app joins with a
number or a name (`" chapters"`, `"Synced "`); keep their leading and
trailing spaces.

## Adding a language

1. Add `${PROJECT_SOURCE_DIR}/i18n/scancore_<code>.ts` to
   `qt_add_translations` in `src/CMakeLists.txt` and the language, by its
   own name, to `kLanguages` in `src/app/Customize.cpp`.
2. Run `cmake --build <build dir> --target update_translations`, which
   creates the file with every text to translate.
3. Translate, build and pick it in Settings.

Messages that come from the app's C++ code (most error messages) are not
translated yet.
