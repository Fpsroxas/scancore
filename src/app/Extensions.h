#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>

namespace sc {

// Optional add-ons downloaded on request: the Extensions runtime (desktop),
// which runs Mihon extensions, and the extended novel source pack, a
// private Python and lightnovel-crawler (about 120 MB). Both go into the app
// data folder; nothing is installed system-wide.
class Extensions : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool novelPackAvailable READ novelPackAvailable CONSTANT)
    Q_PROPERTY(bool novelPackInstalled READ novelPackInstalled NOTIFY changed)
    // A runtime exists for this computer (desktop, x64 or Apple arm64).
    Q_PROPERTY(bool runtimeAvailable READ runtimeAvailable CONSTANT)
    // Extensions can run: the runtime is installed (or a development host
    // is built in).
    Q_PROPERTY(bool runtimeInstalled READ runtimeInstalled NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    // What is being installed: "runtime" or "novels".
    Q_PROPERTY(QString busyWith READ busyWith NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    static Extensions *instance();
    static Extensions *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool novelPackAvailable() const;
    bool novelPackInstalled() const;
    bool runtimeAvailable() const;
    bool runtimeInstalled() const;
    bool busy() const { return m_busy; }
    QString busyWith() const { return m_busy ? m_busyWith : QString(); }
    QString status() const { return m_status; }

    Q_INVOKABLE sc::Task *installNovelPack();
    Q_INVOKABLE void removeNovelPack();
    // Downloads (or updates) the Extensions runtime, about 100 MB.
    Q_INVOKABLE sc::Task *installRuntime();
    Q_INVOKABLE void removeRuntime();

    // Sets up the extension host at start-up (main()); stopHost() at exit.
    void startHost();
    void stopHost();

signals:
    void changed();
    void statusChanged(const QString &status);

private:
    // Private so QML uses create(): Qt builds its own instance of a
    // singleton it can default-construct, and C++ and QML would then
    // work on different objects.
    explicit Extensions(QObject *parent = nullptr);

    void setStatus(const QString &status);
    bool m_busy = false;
    QString m_busyWith;
    QString m_status;
};

} // namespace sc
