#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QUrl>
#include <QVariantMap>

namespace sc {

// image://colorwheel/<size> — the hue and saturation disc of the colour
// picker: hue runs counter-clockwise from 3 o'clock, saturation from the
// centre (white) to the rim, at full brightness.
class ColorWheelProvider : public QQuickImageProvider {
public:
    ColorWheelProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

// What Settings needs for themes and shortcuts beyond plain settings:
// background images kept with the app, .sct theme files, key sequences and
// the see-through window.
//
// A .sct file is JSON: {"format": "scancore-theme", "version": 1, "name",
// "colors": {"window", "accent"}, "window": {"opacity", "panels"},
// "background": {"type", "fit", "dim", "blur", "data" (base64)}}. The
// background travels inside it, so one file works on every system.
class Customize : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The window has an alpha channel, so the theme's window transparency
    // can show what is behind it. Decided at start (see main.cpp).
    Q_PROPERTY(bool translucentWindow READ translucentWindow CONSTANT)
    // SCANCORE can start itself again (not inside Flatpak or on phones).
    Q_PROPERTY(bool canRestart READ canRestart CONSTANT)

public:
    static Customize *instance();
    static Customize *create(QQmlEngine *, QJSEngine *) { return instance(); }

    // Whether the saved theme asks for a see-through window; main() then
    // gives windows an alpha channel before creating any.
    static bool savedThemeIsTranslucent();
    static void setTranslucentWindow(bool on);

    bool translucentWindow() const;
    bool canRestart() const;

    // The shortcut for a key press, as Shortcut.sequence takes it ("Ctrl+K");
    // empty for a modifier alone.
    Q_INVOKABLE QString keySequence(int key, int modifiers) const;
    // How a shortcut is written on this system ("⌘K" on macOS).
    Q_INVOKABLE QString keyText(const QString &sequence) const;
    // Whether two shortcuts are the same keys, however they are written.
    Q_INVOKABLE bool sameKeys(const QString &a, const QString &b) const;

    // Copies an image or GIF into the app's theme folder. {ok, url,
    // animated} or {ok: false, error}.
    Q_INVOKABLE QVariantMap useBackground(const QUrl &file);
    // theme: the customTheme setting plus "accent" (#rrggbb).
    Q_INVOKABLE sc::Task *exportTheme(const QVariantMap &theme, const QUrl &file);
    // A theme map ready for the customTheme setting, plus "accent".
    Q_INVOKABLE sc::Task *importTheme(const QUrl &file);

    Q_INVOKABLE void restart();

private:
    explicit Customize(QObject *parent = nullptr) : QObject(parent) {}
};

} // namespace sc
