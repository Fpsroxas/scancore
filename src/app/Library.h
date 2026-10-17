#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

namespace sc {

// Library, reading/watching progress and history for every media type,
// stored in the local database. Series are identified as
// "<media>:<source>:<source url or id>".
class Library : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Bumped on every change so bindings that read the library re-evaluate.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(int unreadUpdates READ unreadUpdates NOTIFY changed)

public:
    explicit Library(QObject *parent = nullptr);
    static Library *instance();
    static Library *create(QQmlEngine *, QJSEngine *) { return instance(); }

    int revision() const { return m_revision; }
    int unreadUpdates() const;

    Q_INVOKABLE static QString idFor(const QString &media, const QString &source, const QString &key);

    Q_INVOKABLE bool isSaved(const QString &seriesId) const;
    Q_INVOKABLE QString category(const QString &seriesId) const;
    Q_INVOKABLE void save(const QVariantMap &series, const QString &category = QStringLiteral("reading"));
    Q_INVOKABLE void remove(const QString &seriesId);
    Q_INVOKABLE void setCategory(const QString &seriesId, const QString &category);
    Q_INVOKABLE void markSeen(const QString &seriesId);

    Q_INVOKABLE sc::Task *entries(const QString &media = QString());
    Q_INVOKABLE sc::Task *history(const QString &media = QString(), int limit = 60);
    Q_INVOKABLE sc::Task *continueItems(const QString &media, int limit = 12);
    Q_INVOKABLE void forget(const QString &seriesId);
    Q_INVOKABLE void clearHistory(const QString &media = QString());

    Q_INVOKABLE void recordProgress(const QVariantMap &series, const QVariantMap &unit, const QVariantMap &position,
                                    double percent, bool completed);
    Q_INVOKABLE void markUnits(const QVariantMap &series, const QVariantList &units, bool completed);
    Q_INVOKABLE QVariantMap progressMap(const QString &seriesId) const;
    Q_INVOKABLE QVariantMap lastProgress(const QString &seriesId) const;
    Q_INVOKABLE QVariantMap seriesRow(const QString &seriesId) const;
    // The chapter list saved the last time the series was opened, so its
    // page can show it before the site answers.
    Q_INVOKABLE QVariantList storedUnits(const QString &seriesId) const { return cachedUnits(seriesId); }

    // Checks saved comics and novels for new chapters.
    Q_INVOKABLE sc::Task *checkUpdates();
    Q_INVOKABLE QVariantMap stats() const;

    // Used by services.
    static void upsertSeries(const QVariantMap &series);
    static void storeUnits(const QString &seriesId, const QVariantList &units);
    static QVariantList cachedUnits(const QString &seriesId);
    void notifyChanged();

signals:
    void changed();
    void updatesFound(int count, const QStringList &titles);

private:
    int m_revision = 0;
};

} // namespace sc
