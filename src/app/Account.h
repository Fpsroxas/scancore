#pragma once

#include "core/Task.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantMap>

namespace sc {

// Optional link to a scancore.onl account. Signing in stores the site's
// session cookie in the app's cookie jar; the password is never saved.
// Sync brings the website library and comic reading history into the local
// library, adds local saves to the website when it knows the series (and
// removes the ones removed here), and keeps anime watch progress and comic
// reading positions in step both ways.
class Account : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    Q_PROPERTY(QVariantMap user READ user NOTIFY changed)
    Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
    Q_PROPERTY(QString lastSync READ lastSync NOTIFY changed)
    // scancore.onl, or a SCANCORE website hosted elsewhere.
    Q_PROPERTY(QString site READ site NOTIFY changed)
    Q_PROPERTY(bool customSite READ customSite NOTIFY changed)

public:
    static Account *instance();
    static Account *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool signedIn() const { return !m_user.isEmpty(); }
    QVariantMap user() const { return m_user; }
    bool syncing() const { return m_syncing; }
    QString lastSync() const;
    QString site() const;
    bool customSite() const;
    // While signed out: the website to sign in to; empty for scancore.onl.
    // Returns what is wrong with the address, or nothing.
    Q_INVOKABLE QString setSite(const QString &address);

    Q_INVOKABLE sc::Task *signIn(const QString &email, const QString &password);
    // Creates a scancore.onl account and signs in.
    Q_INVOKABLE sc::Task *signUp(const QString &email, const QString &username, const QString &password);
    // Website rules, so the form can explain problems before submitting.
    Q_INVOKABLE QVariantList passwordChecks(const QString &password) const;
    Q_INVOKABLE QString usernameProblem(const QString &username) const;
    Q_INVOKABLE void signOut();
    // {libraryImported, historyImported, pushed, morePending}. While a sync
    // runs, returns that one.
    Q_INVOKABLE sc::Task *sync();

signals:
    void changed();

private:
    // Private so QML uses create(): Qt builds its own instance of a
    // singleton it can default-construct, and C++ and QML would then
    // work on different objects.
    explicit Account(QObject *parent = nullptr);

    sc::Task *authenticate(const QString &path, const QJsonObject &body);
    void setUser(const QVariantMap &user);
    // A sync soon after a change in the library or progress, but not more
    // than about once a minute.
    void scheduleSync();
    // Account sync rests while WebDAV or folder sync is on (see Sync).
    bool active() const;
    static QVariantMap importAll(const QJsonObject &session);

    QVariantMap m_user;
    bool m_syncing = false;
    // Something changed during a sync, so another follows it.
    bool m_dirty = false;
    qint64 m_lastSyncEnded = 0;
    QTimer m_soon;
    QPointer<Task> m_task;
};

} // namespace sc
