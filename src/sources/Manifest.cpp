#include "Manifest.h"

#include "core/Http.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QReadWriteLock>
#include <QSaveFile>
#include <QVersionNumber>

namespace sc {

namespace {
QReadWriteLock g_lock;
QJsonObject g_manifest;
QString g_cachePath;
} // namespace

void Manifest::load(const QString &cachePath) {
    g_cachePath = cachePath;
    QFile builtIn(QStringLiteral(":/sources/manifest.json"));
    if (builtIn.open(QIODevice::ReadOnly))
        apply(builtIn.readAll(), false);
    QFile cached(cachePath);
    if (cached.open(QIODevice::ReadOnly))
        apply(cached.readAll(), false);
}

QJsonObject Manifest::current() {
    QReadLocker lock(&g_lock);
    return g_manifest;
}

int Manifest::version() { return current().value(QStringLiteral("version")).toInt(); }

QString Manifest::appVersion() {
    const auto running = QCoreApplication::applicationVersion();
    return running.isEmpty() ? QStringLiteral(SCANCORE_VERSION) : running;
}

bool Manifest::supports(const QJsonObject &entry) {
    const auto wanted = entry.value(QStringLiteral("minVersion")).toString();
    if (wanted.isEmpty())
        return true;
    const auto needed = QVersionNumber::fromString(wanted);
    // A requirement this build cannot read is one it does not meet.
    return !needed.isNull() && QVersionNumber::fromString(appVersion()) >= needed;
}

bool Manifest::bundledEnabled() {
#ifndef SCANCORE_BUNDLED_SOURCES
    return false;
#else
    return current().value(QStringLiteral("bundled")).toObject().value(QStringLiteral("enabled")).toBool(true);
#endif
}

QString Manifest::bundledReason() {
    const auto reason = current().value(QStringLiteral("bundled")).toObject().value(QStringLiteral("reason")).toString();
    return reason.isEmpty() ? QStringLiteral("Built-in sources are off. Add an extension repository in Settings → Extensions to keep reading.") : reason;
}

bool Manifest::apply(const QByteArray &json, bool persist) {
    const auto doc = QJsonDocument::fromJson(json);
    const auto object = doc.object();
    if (!object.value(QStringLiteral("comics")).isArray())
        return false;
    {
        QWriteLocker lock(&g_lock);
        if (!g_manifest.isEmpty() &&
            object.value(QStringLiteral("version")).toInt() <= g_manifest.value(QStringLiteral("version")).toInt())
            return false;
        g_manifest = object;
    }
    if (persist && !g_cachePath.isEmpty()) {
        QSaveFile file(g_cachePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(json);
            file.commit();
        }
    }
    return true;
}

bool Manifest::refresh() {
    const auto response = Http::get(QUrl(QString::fromLatin1(RemoteUrl)), {}, 10000);
    if (!response.ok())
        return false;
    return apply(response.body, true);
}

} // namespace sc
