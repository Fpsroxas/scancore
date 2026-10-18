#pragma once

#include <QLocalServer>
#include <QObject>
#include <QStringList>

namespace sc {

// One SCANCORE per profile. A second launch hands its files and links to the
// one already running (which comes to the front, even from the tray) and
// exits. Profiles are told apart by their data folder, so test profiles run
// side by side.
class SingleInstance : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);

    // True when another SCANCORE took `arguments`; this process should exit.
    bool forwardToRunning(const QStringList &arguments);
    // Starts listening for later launches.
    void listen();

signals:
    // A later launch, with the files and links it was given.
    void activated(const QStringList &arguments);

private:
    QString m_name;
    QLocalServer m_server;
};

} // namespace sc
