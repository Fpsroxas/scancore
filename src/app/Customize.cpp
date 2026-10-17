#include "Customize.h"

#include "core/Database.h"

#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sc {

namespace {

bool g_translucent = false;

// Backgrounds can be large GIFs, but not unbounded.
constexpr qint64 kMaxBackground = 40LL * 1024 * 1024;
constexpr qint64 kMaxThemeFile = kMaxBackground * 4 / 3 + 1024 * 1024;

QString themesFolder() {
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/themes");
    QDir().mkpath(dir);
    return dir;
}

// The picture formats a background may be, told by their first bytes rather
// than by a file name or a type someone wrote down.
QString imageType(const QByteArray &data) {
    if (data.startsWith("\x89PNG\r\n\x1a\n"))
        return QStringLiteral("png");
    if (data.startsWith("\xff\xd8\xff"))
        return QStringLiteral("jpg");
    if (data.startsWith("GIF87a") || data.startsWith("GIF89a"))
        return QStringLiteral("gif");
    if (data.size() > 12 && data.startsWith("RIFF") && data.mid(8, 4) == "WEBP")
        return QStringLiteral("webp");
    return {};
}

QString mimeFor(const QString &type) {
    return type == QLatin1String("jpg") ? QStringLiteral("image/jpeg") : QStringLiteral("image/") + type;
}

// Stores a background under a name taken from its content and removes the
// previous ones, which nothing refers to any more.
QString storeBackground(const QByteArray &data, const QString &type) {
    const auto name = QStringLiteral("background-%1.%2")
                          .arg(QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(16)), type);
    QDir dir(themesFolder());
    const auto path = dir.filePath(name);
    if (!QFileInfo::exists(path)) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
            throw Error(QStringLiteral("Could not save the background in SCANCORE's data folder."));
    }
    for (const auto &old : dir.entryList({QStringLiteral("background-*")}, QDir::Files))
        if (old != name)
            dir.remove(old);
    return path;
}

QString colorOr(const QJsonValue &value, const QString &fallback) {
    const auto text = value.toString();
    return QColor::isValidColorName(text) ? QColor(text).name() : fallback;
}

double clamped(const QJsonValue &value, double low, double high, double fallback) {
    if (!value.isDouble())
        return fallback;
    return std::clamp(value.toDouble(), low, high);
}

} // namespace

// ---------------------------------------------------------------- colour wheel

ColorWheelProvider::ColorWheelProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage ColorWheelProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    const int side = std::clamp(requestedSize.width() > 0 ? requestedSize.width() : id.toInt(), 64, 1024);
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const double centre = (side - 1) / 2.0;
    const double radius = side / 2.0;
    for (int y = 0; y < side; ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < side; ++x) {
            const double dx = x - centre;
            const double dy = centre - y;
            const double distance = std::hypot(dx, dy);
            // One pixel of soft edge.
            const double alpha = std::clamp(radius - distance, 0.0, 1.0);
            if (alpha <= 0)
                continue;
            double hue = std::atan2(dy, dx) / (2 * std::numbers::pi);
            if (hue < 0)
                hue += 1;
            const auto color = QColor::fromHsvF(float(std::min(hue, 0.9999)), float(std::min(1.0, distance / radius)), 1.0f);
            line[x] = qPremultiply(qRgba(color.red(), color.green(), color.blue(), int(alpha * 255)));
        }
    }
    if (size)
        *size = image.size();
    return image;
}

// ---------------------------------------------------------------- Customize

Customize *Customize::instance() {
    static auto *created = [] {
        auto *c = new Customize;
        QJSEngine::setObjectOwnership(c, QJSEngine::CppOwnership);
        return c;
    }();
    return created;
}

bool Customize::savedThemeIsTranslucent() {
    if (Database::pref(QStringLiteral("setting.theme")).toString() != QLatin1String("custom"))
        return false;
    const auto theme = Database::pref(QStringLiteral("setting.customTheme")).toMap();
    return theme.value(QStringLiteral("windowOpacity"), 1.0).toDouble() < 0.999;
}

void Customize::setTranslucentWindow(bool on) { g_translucent = on; }

bool Customize::translucentWindow() const { return g_translucent; }

bool Customize::canRestart() const {
#ifdef Q_OS_ANDROID
    return false;
#else
    return !qEnvironmentVariableIsSet("FLATPAK_ID");
#endif
}

QString Customize::keySequence(int key, int modifiers) const {
    switch (key) {
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_unknown:
    case 0:
        return {};
    default:
        break;
    }
    auto mods = Qt::KeyboardModifiers(modifiers) &
                (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    // A symbol typed with Shift ("?" or "+") is matched without it; the
    // shortcut system tries both ways of typing it.
    if (key >= 0x21 && key <= 0x7e && !QChar(key).isLetterOrNumber())
        mods &= ~Qt::ShiftModifier;
    return QKeySequence(QKeyCombination(mods, Qt::Key(key))).toString(QKeySequence::PortableText);
}

QString Customize::keyText(const QString &sequence) const {
    return QKeySequence::fromString(sequence, QKeySequence::PortableText).toString(QKeySequence::NativeText);
}

bool Customize::sameKeys(const QString &a, const QString &b) const {
    const auto first = QKeySequence::fromString(a, QKeySequence::PortableText);
    return !first.isEmpty() && first == QKeySequence::fromString(b, QKeySequence::PortableText);
}

QVariantMap Customize::useBackground(const QUrl &url) {
    const auto path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {{"ok", false}, {"error", QStringLiteral("Could not open that file.")}};
    if (file.size() > kMaxBackground)
        return {{"ok", false}, {"error", QStringLiteral("That file is larger than 40 MB.")}};
    const auto data = file.readAll();
    const auto type = imageType(data);
    if (type.isEmpty())
        return {{"ok", false}, {"error", QStringLiteral("Choose a PNG, JPEG, GIF or WebP image.")}};
    try {
        const auto stored = storeBackground(data, type);
        return {{"ok", true},
                {"url", QUrl::fromLocalFile(stored).toString()},
                {"animated", type == QLatin1String("gif") || type == QLatin1String("webp")}};
    } catch (const Error &e) {
        return {{"ok", false}, {"error", e.message()}};
    }
}

Task *Customize::exportTheme(const QVariantMap &theme, const QUrl &target) {
    return Task::run([theme, target]() -> QVariant {
        QJsonObject root{{"format", "scancore-theme"},
                         {"version", 1},
                         {"name", theme.value("name").toString().left(80)},
                         {"app", QStringLiteral("SCANCORE ") + QCoreApplication::applicationVersion()},
                         {"colors", QJsonObject{{"window", theme.value("base").toString()},
                                                {"accent", theme.value("accent").toString()}}},
                         {"window", QJsonObject{{"opacity", theme.value("windowOpacity", 1.0).toDouble()},
                                                {"panels", theme.value("panelOpacity", 0.85).toDouble()}}}};
        const QUrl background(theme.value("background").toString());
        if (background.isLocalFile()) {
            QFile file(background.toLocalFile());
            if (file.open(QIODevice::ReadOnly) && file.size() <= kMaxBackground) {
                const auto data = file.readAll();
                const auto type = imageType(data);
                if (!type.isEmpty())
                    root.insert("background", QJsonObject{{"type", mimeFor(type)},
                                                          {"fit", theme.value("backgroundFit", "cover").toString()},
                                                          {"dim", theme.value("backgroundDim", 0.45).toDouble()},
                                                          {"blur", theme.value("backgroundBlur", 0.0).toDouble()},
                                                          {"data", QString::fromLatin1(data.toBase64())}});
            }
        }
        auto path = target.isLocalFile() ? target.toLocalFile() : target.toString();
        if (!path.endsWith(QLatin1String(".sct"), Qt::CaseInsensitive))
            path += QStringLiteral(".sct");
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            throw Error(QStringLiteral("Could not write %1.").arg(QFileInfo(path).fileName()));
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        if (!file.commit())
            throw Error(QStringLiteral("Could not write %1.").arg(QFileInfo(path).fileName()));
        return QFileInfo(path).fileName();
    });
}

Task *Customize::importTheme(const QUrl &source) {
    return Task::run([source]() -> QVariant {
        const auto path = source.isLocalFile() ? source.toLocalFile() : source.toString();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            throw Error(QStringLiteral("Could not open that theme file."));
        if (file.size() > kMaxThemeFile)
            throw Error(QStringLiteral("That theme file is too large."));
        QJsonParseError parseError{};
        const auto root = QJsonDocument::fromJson(file.readAll(), &parseError).object();
        if (parseError.error != QJsonParseError::NoError || root.value("format").toString() != QLatin1String("scancore-theme"))
            throw Error(QStringLiteral("That is not a SCANCORE theme."));
        if (root.value("version").toInt() > 1)
            throw Error(QStringLiteral("That theme was made by a newer SCANCORE. Update to use it."));

        const auto colors = root.value("colors").toObject();
        const auto window = root.value("window").toObject();
        QVariantMap theme{{"name", root.value("name").toString().left(80)},
                          {"base", colorOr(colors.value("window"), QStringLiteral("#14161c"))},
                          {"accent", colorOr(colors.value("accent"), QString())},
                          {"windowOpacity", clamped(window.value("opacity"), 0.3, 1.0, 1.0)},
                          {"panelOpacity", clamped(window.value("panels"), 0.2, 1.0, 0.85)}};
        const auto background = root.value("background").toObject();
        if (!background.isEmpty()) {
            const auto encoded = background.value("data").toString().toLatin1();
            const auto data = QByteArray::fromBase64(encoded);
            const auto type = imageType(data);
            if (data.size() > kMaxBackground || type.isEmpty())
                throw Error(QStringLiteral("The theme's background is not a usable image."));
            const auto fit = background.value("fit").toString();
            theme.insert("background", QUrl::fromLocalFile(storeBackground(data, type)).toString());
            theme.insert("backgroundAnimated", type == QLatin1String("gif") || type == QLatin1String("webp"));
            theme.insert("backgroundFit", QStringList{"cover", "contain", "tile", "stretch"}.contains(fit) ? fit : QStringLiteral("cover"));
            theme.insert("backgroundDim", clamped(background.value("dim"), 0.0, 0.95, 0.45));
            theme.insert("backgroundBlur", clamped(background.value("blur"), 0.0, 1.0, 0.0));
        }
        return theme;
    });
}

void Customize::restart() {
    if (!canRestart())
        return;
    // An AppImage runs from a temporary mount; start the image itself.
    auto program = qEnvironmentVariable("APPIMAGE");
    if (program.isEmpty())
        program = QCoreApplication::applicationFilePath();
    if (QProcess::startDetached(program, QCoreApplication::arguments().mid(1)))
        QCoreApplication::quit();
}

} // namespace sc
