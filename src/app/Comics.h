#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>

namespace sc {

class Comics : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Comics(QObject *parent = nullptr) : QObject(parent) {}

    // [{key, label, enabled, disabledReason, nsfw, hasLatest}]
    Q_INVOKABLE QVariantList sources() const;
    Q_INVOKABLE QString sourceLabel(const QString &key) const;
    Q_INVOKABLE QString sourceForUrl(const QString &url) const;
    Q_INVOKABLE QString seriesId(const QString &url) const;

    // {comics: [...], providers: [{source, label, ok, count, error}]}
    Q_INVOKABLE sc::Task *search(const QString &query, const QString &source = QStringLiteral("all"));
    // Titles from Comick's catalogue, which knows far more series (and their
    // other names) than any one site: {comics: [{hid, title, titles, cover,
    // type, status, year, ...}], providers: []}.
    Q_INVOKABLE sc::Task *catalog(const QString &query);
    // Comick's ranked shelves: {trending, popular, newest, completed}.
    Q_INVOKABLE sc::Task *catalogFeeds(bool mature = false);
    // Where a catalogue title can be read: the series on SCANCORE's sources
    // listed under any of its names. {matches: [{source, sourceLabel, url,
    // title, cover}]}
    Q_INVOKABLE sc::Task *resolve(const QVariantMap &item);
    // {series: {...}, chapters: [...], offline: bool, error?: string}
    Q_INVOKABLE sc::Task *series(const QString &url, const QVariantMap &summary = {});
    // {panels: [{url, width, height}], referer, offline}
    Q_INVOKABLE sc::Task *panels(const QString &chapterUrl, const QString &seriesId = QString(),
                                 const QString &unitKey = QString());
    // {updates: [...], providers: [...]}
    Q_INVOKABLE sc::Task *latest(int limit = 40);

    Q_INVOKABLE QString imageReferer(const QString &url) const;
    // Warms the image cache for the first pages of an upcoming chapter.
    Q_INVOKABLE void prefetch(const QVariantList &panels, const QString &referer, int count = 6);

    // Chapter list as library units ({key, number, title, url, released}).
    static QVariantList fetchUnits(const QString &url);
};

} // namespace sc
