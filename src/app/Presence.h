#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

namespace sc {

// Discord Rich Presence: "Reading Solo Leveling · Chapter 12" on the user's
// Discord profile, through the Discord app's local socket. Off unless turned
// on in Settings → Connections, and titles can stay private. Needs the
// SCANCORE application id on Discord (SCANCORE_DISCORD_APP_ID at build time).
class Presence : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)

public:
    static Presence *instance();
    static Presence *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool available() const;
    bool connected() const { return m_ready; }

    // media: "comic", "novel" or "anime"; title and unit ("Chapter 12").
    Q_INVOKABLE void show(const QString &media, const QString &title, const QString &unit);
    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    explicit Presence(QObject *parent = nullptr);
    bool enabled() const;
    void connectToDiscord();
    void send(int opcode, const QJsonObject &payload);
    void publish();

    QLocalSocket m_socket;
    QStringList m_candidates;
    bool m_ready = false;
    QByteArray m_buffer;
    QVariantMap m_current;
    qint64 m_startedAt = 0;
    QTimer m_retry;
};

} // namespace sc
