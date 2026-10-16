#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>

namespace sc {

// Optional add-ons downloaded on request. Today: the extended novel source
// pack, which installs a private Python and lightnovel-crawler (about 120
// MB) into the app data folder. Nothing is installed system-wide.
class Extensions : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool novelPackAvailable READ novelPackAvailable CONSTANT)
    Q_PROPERTY(bool novelPackInstalled READ novelPackInstalled NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    explicit Extensions(QObject *parent = nullptr) : QObject(parent) {}

    bool novelPackAvailable() const;
    bool novelPackInstalled() const;
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }

    Q_INVOKABLE sc::Task *installNovelPack();
    Q_INVOKABLE void removeNovelPack();

signals:
    void changed();
    void statusChanged(const QString &status);

private:
    void setStatus(const QString &status);
    bool m_busy = false;
    QString m_status;
};

} // namespace sc
