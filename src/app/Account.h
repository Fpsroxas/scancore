#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

namespace sc {

// Optional link to a scancore.onl account. Signing in stores the site's
// session cookie in the app's cookie jar; the password is never saved.
// Sync brings the website library and comic reading history into the local
// library, adds local saves to the website when it knows the series, and
// keeps anime watch progress in step both ways.
class Account : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    Q_PROPERTY(QVariantMap user READ user NOTIFY changed)
    Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
    Q_PROPERTY(QString lastSync READ lastSync NOTIFY changed)
    Q_PROPERTY(QString site READ site CONSTANT)

public:
    explicit Account(QObject *parent = nullptr);
    static Account *instance();
    static Account *create(QQmlEngine *, QJSEngine *) { return instance(); }

    bool signedIn() const { return !m_user.isEmpty(); }
    QVariantMap user() const { return m_user; }
    bool syncing() const { return m_syncing; }
    QString lastSync() const;
    QString site() const { return QStringLiteral("https://scancore.onl"); }

    Q_INVOKABLE sc::Task *signIn(const QString &email, const QString &password);
    // Creates a scancore.onl account and signs in.
    Q_INVOKABLE sc::Task *signUp(const QString &email, const QString &username, const QString &password);
    // Website rules, so the form can explain problems before submitting.
    Q_INVOKABLE QVariantList passwordChecks(const QString &password) const;
    Q_INVOKABLE QString usernameProblem(const QString &username) const;
    Q_INVOKABLE void signOut();
    // {libraryImported, historyImported, pushed, morePending}
    Q_INVOKABLE sc::Task *sync();

signals:
    void changed();

private:
    sc::Task *authenticate(const QString &path, const QJsonObject &body);
    void setUser(const QVariantMap &user);
    static QVariantMap importAll(const QJsonObject &session);

    QVariantMap m_user;
    bool m_syncing = false;
};

} // namespace sc
