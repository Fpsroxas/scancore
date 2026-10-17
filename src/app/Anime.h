#pragma once

#include "core/Task.h"

#include <QObject>
#include <QQmlEngine>

namespace sc {

// AniList metadata, ani.zip episode details, AniSkip ranges and ReAnime
// player links. Anime series are stored as "anime:anilist:<id>".
class Anime : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool webPlayerAvailable READ webPlayerAvailable CONSTANT)
    Q_PROPERTY(bool nativePlayerAvailable READ nativePlayerAvailable CONSTANT)
    // Which embedded browser shows the player: "system" (the platform's own
    // web view: Android WebView, WebView2 on Windows, WKWebView on macOS),
    // "webengine" (Qt WebEngine) or "" when there is none.
    Q_PROPERTY(QString playerEngine READ playerEngine CONSTANT)

public:
    explicit Anime(QObject *parent = nullptr) : QObject(parent) {}

    bool webPlayerAvailable() const;
    bool nativePlayerAvailable() const;
    QString playerEngine() const;

    Q_INVOKABLE QString seriesId(int anilistId) const;
    Q_INVOKABLE QStringList genres() const;

    // {trending, popular, season, top}
    Q_INVOKABLE sc::Task *home();
    // {results, hasNextPage}
    Q_INVOKABLE sc::Task *search(const QVariantMap &filters);
    Q_INVOKABLE sc::Task *media(int anilistId);
    Q_INVOKABLE sc::Task *episodes(int anilistId);
    Q_INVOKABLE sc::Task *recommendations(int anilistId);
    Q_INVOKABLE sc::Task *schedule(qint64 start, qint64 end);
    // {streams: [{url, quality, type}], message}
    Q_INVOKABLE sc::Task *streams(int anilistId, int episode);
    Q_INVOKABLE sc::Task *skipTimes(int anilistId, int episode, int duration);

    // Library series map for an AniList media map.
    Q_INVOKABLE QVariantMap seriesFor(const QVariantMap &media) const;
    Q_INVOKABLE bool isEmbedAllowed(const QString &url) const;
};

} // namespace sc
