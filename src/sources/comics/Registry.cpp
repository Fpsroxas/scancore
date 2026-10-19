#include "Registry.h"

#include "extensions/ExtensionSource.h"
#include "extensions/Host.h"
#include "sources/Manifest.h"
#include "sources/local/LocalSources.h"

#include <QJsonArray>
#include <QReadWriteLock>

namespace sc::comics {

namespace {
QReadWriteLock g_lock;
QList<std::shared_ptr<Source>> g_sources;
} // namespace

void Registry::rebuild() {
    QList<std::shared_ptr<Source>> sources;
    for (const auto &value : Manifest::current().value(QStringLiteral("comics")).toArray()) {
        const auto o = value.toObject();
        if (!Manifest::supports(o))
            continue;
        SourceConfig config;
        config.key = o.value(QStringLiteral("key")).toString();
        config.label = o.value(QStringLiteral("label")).toString(config.key);
        config.engine = o.value(QStringLiteral("engine")).toString();
        config.base = o.value(QStringLiteral("base")).toString();
        config.api = o.value(QStringLiteral("api")).toString(config.base);
        for (const auto &host : o.value(QStringLiteral("hosts")).toArray())
            config.hosts.append(host.toString().toLower());
        config.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        config.nsfw = o.value(QStringLiteral("nsfw")).toBool(false);
        config.disabledReason = o.value(QStringLiteral("disabledReason")).toString();
        config.options = o.value(QStringLiteral("options")).toObject();
        config.language = o.value(QStringLiteral("language")).toString(QStringLiteral("en"));
        config.urlTag = o.value(QStringLiteral("urlTag")).toString();
        if (config.key.isEmpty() || config.base.isEmpty())
            continue;
        if (auto source = createSource(config))
            sources.append(std::shared_ptr<Source>(std::move(source)));
    }
    // Sources of installed Mihon extensions.
    for (const auto &entry : ext::Manager::sources(ext::Kind::Mihon))
        sources.append(std::make_shared<ext::ExtensionSource>(entry));
    // Files on this device: opens any file, and lists the chosen folders.
    SourceConfig local;
    local.key = QStringLiteral("local");
    local.label = QStringLiteral("On this device");
    local.engine = QStringLiteral("local");
    local.base = QStringLiteral("file://");
    sources.append(std::shared_ptr<Source>(local::createComicSource(local)));
    QWriteLocker lock(&g_lock);
    g_sources = sources;
}

QList<std::shared_ptr<Source>> Registry::all() {
    QReadLocker lock(&g_lock);
    return g_sources;
}

std::shared_ptr<Source> Registry::byKey(const QString &key) {
    for (const auto &source : all())
        if (source->key() == key)
            return source;
    return nullptr;
}

std::shared_ptr<Source> Registry::forUrl(const QString &url) {
    // Extension links name their source ("mihon:<id>:<link>").
    if (const auto wrapped = ext::unwrapUrl(url))
        return byKey(wrapped->key);
    return sourceForUrl(all(), url);
}

} // namespace sc::comics
