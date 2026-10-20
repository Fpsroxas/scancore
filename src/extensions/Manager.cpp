#include "Manager.h"

#include "Apk.h"
#include "Host.h"
#include "Index.h"
#include "core/Database.h"
#include "core/Http.h"
#include "core/Text.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrlQuery>
#include <QWaitCondition>

namespace sc::ext {

namespace {

struct RepoState {
    Repository repo;
    QList<Package> packages;
    QString etag;
    QString modified;
    QString error;
};

struct Trust {
    qint64 versionCode = 0;
    QString signer;
};

struct State {
    bool loaded = false;
    QList<RepoState> repos;
    QHash<QString, Installed> installed;          // kind:pkg
    QHash<QString, Trust> trust;                  // kind:pkg
    QHash<QString, bool> enabled;                 // source key
    QHash<QString, QString> errors;               // source key
};

QReadWriteLock g_lock;
State g_state;
QString g_storage;
QMutex g_callbackMutex;
std::function<void()> g_changed;
// One install of a package at a time; different packages download in
// parallel (a new repository installs many at once).
QMutex g_busyMutex;
QWaitCondition g_busyFree;
QSet<QString> g_busy;

class BusyPackage {
public:
    explicit BusyPackage(QString key) : m_key(std::move(key)) {
        QMutexLocker lock(&g_busyMutex);
        while (g_busy.contains(m_key))
            g_busyFree.wait(&g_busyMutex);
        g_busy.insert(m_key);
    }
    ~BusyPackage() {
        QMutexLocker lock(&g_busyMutex);
        g_busy.remove(m_key);
        g_busyFree.wakeAll();
    }

private:
    QString m_key;
};

QString id(Kind kind, const QString &pkg) { return kindName(kind) + QLatin1Char(':') + pkg; }

QJsonArray packagesJson(const QList<Package> &packages) {
    QJsonArray out;
    for (const auto &p : packages)
        out.append(p.toJson());
    return out;
}

QList<Package> packagesFrom(const QByteArray &json) {
    QList<Package> out;
    for (const auto &value : QJsonDocument::fromJson(json).array())
        out.append(Package::fromJson(value.toObject()));
    return out;
}

QByteArray compact(const QJsonValue &value) {
    return value.isArray() ? QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)
                           : QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
}

// Loads everything from the database once; callers hold the write lock.
void ensureLoaded() {
    if (g_state.loaded)
        return;
    State state;
    state.loaded = true;
    // Tools and tests without a library have no extensions.
    if (Database::path().isEmpty()) {
        g_state = state;
        return;
    }
    for (const auto &row : Database::query(QStringLiteral(
             "SELECT url, info, packages, etag, modified, error FROM ext_repos ORDER BY added_at"))) {
        const auto r = row.toMap();
        RepoState repo;
        repo.repo = Repository::fromJson(QJsonDocument::fromJson(r.value("info").toByteArray()).object());
        repo.repo.url = r.value("url").toString();
        repo.packages = packagesFrom(r.value("packages").toByteArray());
        repo.etag = r.value("etag").toString();
        repo.modified = r.value("modified").toString();
        repo.error = r.value("error").toString();
        state.repos.append(repo);
    }
    for (const auto &row : Database::query(QStringLiteral(
             "SELECT kind, pkg, package, repo_url, signers, path, installed_at FROM ext_installed"))) {
        const auto r = row.toMap();
        Installed i;
        i.package = Package::fromJson(QJsonDocument::fromJson(r.value("package").toByteArray()).object());
        i.repoUrl = r.value("repo_url").toString();
        i.signers = r.value("signers").toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
        i.path = r.value("path").toString();
        i.installedAt = r.value("installed_at").toLongLong();
        state.installed.insert(id(i.package.kind, i.package.pkg), i);
    }
    for (const auto &row : Database::query(QStringLiteral("SELECT kind, pkg, version_code, signer FROM ext_trust"))) {
        const auto r = row.toMap();
        state.trust.insert(r.value("kind").toString() + QLatin1Char(':') + r.value("pkg").toString(),
                           {r.value("version_code").toLongLong(), r.value("signer").toString()});
    }
    for (const auto &row : Database::query(QStringLiteral("SELECT key, enabled, error FROM ext_sources"))) {
        const auto r = row.toMap();
        state.enabled.insert(r.value("key").toString(), r.value("enabled").toBool());
        if (!r.value("error").toString().isEmpty())
            state.errors.insert(r.value("key").toString(), r.value("error").toString());
    }
    state.loaded = true;
    g_state = state;
}

// Read access with the state loaded.
template <typename F>
auto reading(F &&fn) {
    {
        QReadLocker lock(&g_lock);
        if (g_state.loaded)
            return fn(std::as_const(g_state));
    }
    QWriteLocker lock(&g_lock);
    ensureLoaded();
    return fn(std::as_const(g_state));
}

template <typename F>
auto writing(F &&fn) {
    QWriteLocker lock(&g_lock);
    ensureLoaded();
    return fn(g_state);
}

void changed() {
    std::function<void()> callback;
    {
        QMutexLocker lock(&g_callbackMutex);
        callback = g_changed;
    }
    if (callback)
        callback();
}

// Signed with a repository's key, or trusted by hand at this version and key.
bool isTrusted(const State &state, const Installed &i) {
    if (i.package.kind == Kind::LNReader)
        return std::any_of(state.repos.begin(), state.repos.end(),
                           [&](const RepoState &r) { return r.repo.kind == Kind::LNReader && r.repo.url == i.repoUrl; });
    if (i.signers.isEmpty())
        return false;
    for (const auto &r : state.repos)
        if (r.repo.kind == i.package.kind && !r.repo.signingKey.isEmpty() && i.signers.contains(r.repo.signingKey))
            return true;
    const auto t = state.trust.value(id(i.package.kind, i.package.pkg));
    return t.versionCode == i.package.versionCode && t.signer == i.signers.last();
}

bool allowedHost(const QUrl &url) {
    if (url.scheme() == QLatin1String("https"))
        return true;
    // Plain http only for development servers; extension code otherwise
    // comes over https.
    const auto host = url.host();
    return url.scheme() == QLatin1String("http") &&
           (host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1") || host.endsWith(QLatin1String(".test")));
}

QUrl repositoryUrl(QString text) {
    text = text.trimmed();
    if (!text.contains(QLatin1String("://")))
        text.prepend(QStringLiteral("https://"));
    QUrl url(text);
    // A folder: its index.
    if (url.path().isEmpty() || url.path().endsWith(QLatin1Char('/')))
        url.setPath(url.path() + (url.path().isEmpty() ? QStringLiteral("/") : QString()) + QStringLiteral("index.min.json"));
    return url;
}

// The files to try for a link: the link itself, or, for a repository's page
// on GitHub (github.com/<owner>/<name>, perhaps /tree/<branch>), its index on
// that branch, else on "repo" (where Mihon and Aniyomi repositories publish
// it), "main" or "master".
QList<QUrl> repositoryCandidates(const QString &text) {
    const auto url = repositoryUrl(text);
    const auto host = url.host().toLower();
    if (host != QLatin1String("github.com") && host != QLatin1String("www.github.com"))
        return {url};
    auto parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!parts.isEmpty() && parts.last() == QLatin1String("index.min.json") && parts.size() == 3)
        parts.removeLast(); // github.com/<owner>/<name>/ became …/index.min.json above
    if (parts.size() < 2 || (parts.size() > 2 && parts.at(2) != QLatin1String("tree")))
        return {url};
    const auto repo = parts.at(0) + QLatin1Char('/') + QString(parts.at(1)).remove(QRegularExpression(QStringLiteral("\\.git$")));
    QStringList branches{QStringLiteral("repo"), QStringLiteral("main"), QStringLiteral("master")};
    QString folder;
    if (parts.size() > 3) {
        branches.prepend(parts.at(3));
        branches.removeDuplicates();
        folder = parts.mid(4).join(QLatin1Char('/'));
    }
    QList<QUrl> out;
    for (const auto &branch : branches)
        out.append(QUrl(QStringLiteral("https://raw.githubusercontent.com/%1/%2/%3index.min.json")
                            .arg(repo, branch, folder.isEmpty() ? QString() : folder + QLatin1Char('/'))));
    return out;
}

Response fetch(const QUrl &url, const QString &etag = {}, const QString &modified = {}) {
    if (!url.isValid() || !allowedHost(url))
        throw Error(QStringLiteral("Repositories must be https links."));
    Request request;
    request.url = url;
    request.timeoutMs = 30000;
    request.maxBytes = 64 * 1024 * 1024;
    request.headers = {{"Accept", "application/json, application/x-protobuf, */*"}};
    if (!etag.isEmpty())
        request.headers.append({"If-None-Match", etag.toUtf8()});
    if (!modified.isEmpty())
        request.headers.append({"If-Modified-Since", modified.toUtf8()});
    const auto response = Http::send(request);
    if (response.status == 304)
        return response;
    if (response.status == 404)
        throw Error(QStringLiteral("There is no repository at %1.").arg(url.toString()), 404);
    if (!response.ok())
        throw Error(QStringLiteral("%1 answered HTTP %2.").arg(url.host()).arg(response.status), response.status);
    return response;
}

// Reads a repository: follows repo.json, index_v2 and extension lists until
// it has the repository's details and its extensions.
RepoState read(const QUrl &start, std::optional<Kind> hint) {
    RepoState state;
    std::optional<Repository> repo;
    std::optional<QList<Package>> packages;
    QUrl next = start;
    QString listEtag, listModified;
    for (int step = 0; step < 4 && next.isValid(); ++step) {
        const auto response = fetch(next);
        const auto doc = parseIndex(response.body, next, hint);
        if (doc.type == IndexDocument::Type::Unknown)
            throw Error(step == 0 ? QStringLiteral("That link is not an extension or plugin repository.")
                                  : QStringLiteral("The repository's files could not be read."));
        if (doc.repo && !repo)
            repo = doc.repo;
        if (doc.hasPackages && (!packages || doc.type == IndexDocument::Type::ExtensionList ||
                                doc.type == IndexDocument::Type::Store)) {
            packages = doc.packages;
            listEtag = QString::fromUtf8(response.header("ETag"));
            listModified = QString::fromUtf8(response.header("Last-Modified"));
        }
        // A legacy repository with a newer store (index_v2) is read as the
        // store.
        const bool better = doc.type == IndexDocument::Type::LegacyRepo && !doc.follow.endsWith(QLatin1String("index.min.json"));
        if (better) {
            repo.reset();
            packages.reset();
        } else if (repo && packages) {
            break;
        }
        next = doc.follow.isEmpty() ? QUrl() : QUrl(doc.follow);
    }
    if (!repo || !packages)
        throw Error(QStringLiteral("The repository's files could not be read."));
    if (repo->kind != Kind::LNReader && repo->signingKey.isEmpty())
        throw Error(QStringLiteral("The repository has no signing key, so its extensions cannot be checked."));
    // One kind per repository: the hint, else what most extensions are.
    Kind kind = hint.value_or(repo->kind);
    if (!hint && repo->kind != Kind::LNReader) {
        int anime = 0;
        for (const auto &p : *packages)
            anime += p.kind == Kind::Aniyomi;
        kind = anime * 2 > packages->size() ? Kind::Aniyomi : Kind::Mihon;
    }
    repo->kind = kind;
    for (auto &p : *packages)
        p.kind = kind;
    state.repo = *repo;
    state.packages = *packages;
    state.etag = listEtag;
    state.modified = listModified;
    return state;
}

void storeRepo(const RepoState &r, bool fresh) {
    if (fresh)
        Database::exec(QStringLiteral("INSERT INTO ext_repos(url, kind, info, packages, etag, modified, refreshed_at, added_at) "
                                      "VALUES(?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(url) DO UPDATE SET kind = excluded.kind, "
                                      "info = excluded.info, packages = excluded.packages, etag = excluded.etag, "
                                      "modified = excluded.modified, refreshed_at = excluded.refreshed_at, error = NULL"),
                       {r.repo.url, kindName(r.repo.kind), compact(r.repo.toJson()), compact(packagesJson(r.packages)),
                        r.etag, r.modified, Database::now(), Database::now()});
    else
        Database::exec(QStringLiteral("UPDATE ext_repos SET packages = ?, etag = ?, modified = ?, refreshed_at = ?, error = ? WHERE url = ?"),
                       {compact(packagesJson(r.packages)), r.etag, r.modified, Database::now(),
                        r.error.isEmpty() ? QVariant() : QVariant(r.error), r.repo.url});
}

QString contentBase(const QString &lang) { return baseLanguage(lang); }

bool inLanguage(const QString &lang, const QString &language) {
    const auto base = contentBase(lang);
    return base == language || base == QLatin1String("all") || base == QLatin1String("multi");
}

QString libProblem(const Package &p) {
    switch (libSupport(p.kind, p.lib)) {
    case LibSupport::Supported: return {};
    case LibSupport::TooNew: return QStringLiteral("%1 needs a newer SCANCORE. Update SCANCORE to use it.").arg(p.name);
    case LibSupport::TooOld: return QStringLiteral("%1 is too old for SCANCORE. Its repository may have a newer version.").arg(p.name);
    case LibSupport::Unknown: return QStringLiteral("%1 uses an extension version SCANCORE does not know (%2).").arg(p.name, p.lib);
    }
    return {};
}

} // namespace

void Manager::setStorage(const QString &folder) { g_storage = folder; }
QString Manager::storage() { return g_storage; }

void Manager::setChangedCallback(std::function<void()> callback) {
    QMutexLocker lock(&g_callbackMutex);
    g_changed = std::move(callback);
}

void Manager::reload() {
    QWriteLocker lock(&g_lock);
    g_state = State();
}

// ---------------------------------------------------------------- repositories

RepositoryPreview Manager::preview(const QString &text, std::optional<Kind> hint) {
    const auto candidates = repositoryCandidates(text);
    const auto &url = candidates.first();
    if (!url.isValid() || url.host().isEmpty())
        throw Error(QStringLiteral("That is not a link."));
    RepoState state;
    for (int i = 0; i < candidates.size(); ++i) {
        try {
            state = read(candidates.at(i), hint);
            break;
        } catch (const Error &e) {
            // A branch without the index: the next one.
            if (e.status() != 404 || i + 1 == candidates.size())
                throw;
        }
    }
    RepositoryPreview preview;
    preview.repo = state.repo;
    preview.packages = state.packages;
    preview.autoInstall = autoInstallSelection(state.packages, contentLanguage(),
                                               Database::pref(QStringLiteral("setting.showNsfwSources"), false).toBool());
    preview.repo.listUrl = state.repo.listUrl.isEmpty() ? state.repo.url : state.repo.listUrl;
    return preview;
}

Repository Manager::add(const RepositoryPreview &preview) {
    RepoState state;
    state.repo = preview.repo;
    state.packages = preview.packages;
    storeRepo(state, true);
    writing([&](State &s) {
        s.repos.erase(std::remove_if(s.repos.begin(), s.repos.end(), [&](const RepoState &r) { return r.repo.url == state.repo.url; }),
                      s.repos.end());
        s.repos.append(state);
        return 0;
    });
    changed();
    return state.repo;
}

void Manager::removeRepository(const QString &url) {
    Database::exec(QStringLiteral("DELETE FROM ext_repos WHERE url = ?"), {url});
    writing([&](State &s) {
        s.repos.erase(std::remove_if(s.repos.begin(), s.repos.end(), [&](const RepoState &r) { return r.repo.url == url; }),
                      s.repos.end());
        return 0;
    });
    // Its extensions stay installed; those it vouched for need trust again.
    changed();
}

QList<Repository> Manager::repositories() {
    return reading([](const State &s) {
        QList<Repository> out;
        for (const auto &r : s.repos)
            out.append(r.repo);
        return out;
    });
}

QList<RepositoryStatus> Manager::repositoryStatus() {
    return reading([](const State &s) {
        QList<RepositoryStatus> out;
        for (const auto &r : s.repos)
            out.append({r.repo, int(r.packages.size()), r.error});
        return out;
    });
}

std::optional<RepositoryLink> Manager::parseLink(const QString &link) {
    const QUrl url(link.trimmed());
    const auto scheme = url.scheme().toLower();
    // The action is the host and path together ("add-repo", "repos/add").
    const auto action = (url.host() + url.path()).toLower().remove(QLatin1Char('/'));
    std::optional<Kind> kind;
    if (scheme == QLatin1String("tachiyomi") || scheme == QLatin1String("mihon"))
        kind = Kind::Mihon;
    else if (scheme == QLatin1String("aniyomi"))
        kind = Kind::Aniyomi;
    else if (scheme == QLatin1String("lnreader"))
        kind = Kind::LNReader;
    else if (scheme != QLatin1String("scancore"))
        return std::nullopt;
    const bool adds = action == QLatin1String("add-repo") || action == QLatin1String("addrepo") ||
                      action == QLatin1String("reposadd") || action == QLatin1String("repoadd") ||
                      action == QLatin1String("extension-store") || action == QLatin1String("add-extension-store");
    if (!adds)
        return std::nullopt;
    const auto target = QUrlQuery(url).queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded).trimmed();
    if (target.isEmpty())
        return std::nullopt;
    if (scheme == QLatin1String("scancore"))
        kind = kindFrom(QUrlQuery(url).queryItemValue(QStringLiteral("kind")));
    return RepositoryLink{target, kind};
}

bool Manager::refresh(const QString &url) {
    auto current = reading([&](const State &s) -> std::optional<RepoState> {
        for (const auto &r : s.repos)
            if (r.repo.url == url)
                return r;
        return std::nullopt;
    });
    if (!current)
        throw Error(QStringLiteral("That repository is not added."));
    RepoState next = *current;
    try {
        const QUrl listUrl(current->repo.listUrl.isEmpty() ? current->repo.url : current->repo.listUrl);
        const auto response = fetch(listUrl, current->etag, current->modified);
        if (response.status == 304) {
            Database::exec(QStringLiteral("UPDATE ext_repos SET refreshed_at = ?, error = NULL WHERE url = ?"), {Database::now(), url});
            return false;
        }
        const auto doc = parseIndex(response.body, listUrl, current->repo.kind);
        if (!doc.hasPackages)
            throw Error(QStringLiteral("The repository's list could not be read."));
        // A store whose key changed is not taken at its word.
        if (doc.repo && current->repo.kind != Kind::LNReader && !doc.repo->signingKey.isEmpty() &&
            doc.repo->signingKey != current->repo.signingKey)
            throw Error(QStringLiteral("The repository's signing key changed. Remove it and add it again if you trust the new key."));
        next.packages = doc.packages;
        for (auto &p : next.packages)
            p.kind = current->repo.kind;
        next.etag = QString::fromUtf8(response.header("ETag"));
        next.modified = QString::fromUtf8(response.header("Last-Modified"));
        next.error.clear();
    } catch (const Error &e) {
        next.error = e.message();
        storeRepo(next, false);
        writing([&](State &s) {
            for (auto &r : s.repos)
                if (r.repo.url == url)
                    r.error = next.error;
            return 0;
        });
        throw;
    }
    storeRepo(next, false);
    writing([&](State &s) {
        for (auto &r : s.repos)
            if (r.repo.url == url)
                r = next;
        return 0;
    });
    changed();
    return true;
}

void Manager::refreshAll() {
    for (const auto &repo : repositories())
        try {
            refresh(repo.url);
        } catch (const Error &) {
            // Kept with the repository as its error.
        }
}

QList<Package> Manager::available(std::optional<Kind> kind) {
    return reading([&](const State &s) {
        QHash<QString, int> at;
        QList<Package> out;
        for (const auto &r : s.repos) {
            if (kind && r.repo.kind != *kind)
                continue;
            for (const auto &p : r.packages) {
                const auto key = id(p.kind, p.pkg);
                if (const auto i = at.value(key, -1); i >= 0) {
                    if (p.versionCode > out[i].versionCode)
                        out[i] = p;
                } else {
                    at.insert(key, int(out.size()));
                    out.append(p);
                }
            }
        }
        return out;
    });
}

QString Manager::repositoryOf(Kind kind, const QString &pkg) {
    return reading([&](const State &s) {
        QString best;
        qint64 version = -1;
        for (const auto &r : s.repos)
            if (r.repo.kind == kind)
                for (const auto &p : r.packages)
                    if (p.pkg == pkg && p.versionCode > version) {
                        version = p.versionCode;
                        best = r.repo.url;
                    }
        return best;
    });
}

QList<Package> Manager::autoInstallSelection(const QList<Package> &packages, const QString &language, bool adult) {
    QList<Package> out;
    for (const auto &p : packages) {
        if (p.torrent || (!adult && p.warning == ContentWarning::Nsfw) || !libProblem(p).isEmpty())
            continue;
        const auto base = contentBase(p.lang);
        bool wanted = base == language;
        // Multi-language extensions when one of their sources is in it.
        if (!wanted && (base == QLatin1String("all") || base == QLatin1String("multi") || base.isEmpty()))
            wanted = std::any_of(p.sources.begin(), p.sources.end(),
                                 [&](const SourceInfo &s) { return contentBase(s.lang) == language; });
        if (wanted)
            out.append(p);
    }
    return out;
}

// ---------------------------------------------------------------- extensions

Installed Manager::install(Kind kind, const QString &pkg) {
    BusyPackage busy(id(kind, pkg));
    const auto repoUrl = repositoryOf(kind, pkg);
    std::optional<Package> listed;
    for (const auto &p : available(kind))
        if (p.pkg == pkg)
            listed = p;
    if (!listed)
        throw Error(QStringLiteral("No repository lists that extension."));
    const auto &package = *listed;
    if (const auto problem = libProblem(package); !problem.isEmpty())
        throw Error(problem);
    if (package.torrent)
        throw Error(QStringLiteral("%1 plays torrents, which SCANCORE cannot.").arg(package.name));
    if (g_storage.isEmpty())
        throw Error(QStringLiteral("Extensions have nowhere to be stored."));

    const QUrl fileUrl(package.fileUrl);
    if (!allowedHost(fileUrl))
        throw Error(QStringLiteral("%1 is not offered over https.").arg(package.name));
    Request request;
    request.url = fileUrl;
    request.timeoutMs = 60000;
    request.maxBytes = 64 * 1024 * 1024;
    const auto response = Http::send(request);
    if (!response.ok() || response.body.isEmpty())
        throw Error(QStringLiteral("%1 could not be downloaded (HTTP %2).").arg(package.name).arg(response.status), response.status);

    QStringList signers;
    if (kind != Kind::LNReader) {
        signers = apkSigners(response.body);
        if (signers.isEmpty())
            throw Error(QStringLiteral("%1 is not signed, so SCANCORE will not install it.").arg(package.name));
        // An update must come from the key that signed what is installed.
        if (const auto old = installedPackage(kind, pkg); old && !old->signers.isEmpty())
            for (const auto &signer : old->signers)
                if (!signers.contains(signer))
                    throw Error(QStringLiteral("This version of %1 is signed with a different key than the one installed. "
                                               "Remove it first if you trust the new key.")
                                    .arg(package.name));
    }

    const auto folder = QStringLiteral("%1/%2/%3").arg(g_storage, kindName(kind), pkg);
    QDir().mkpath(folder);
    const auto path = QStringLiteral("%1/%2.%3").arg(folder).arg(package.versionCode).arg(kind == Kind::LNReader ? "js" : "apk");
    {
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(response.body) != response.body.size() || !file.commit())
            throw Error(QStringLiteral("%1 could not be saved.").arg(package.name));
    }
    // Code is loaded from read-only files (Android 14 refuses writable ones).
    QFile::setPermissions(path, QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
    for (const auto &old : QDir(folder).entryInfoList(QDir::Files))
        if (old.absoluteFilePath() != QFileInfo(path).absoluteFilePath()) {
            QFile::setPermissions(old.absoluteFilePath(), QFile::ReadOwner | QFile::WriteOwner);
            QFile::remove(old.absoluteFilePath());
        }

    Installed installed;
    installed.package = package;
    installed.repoUrl = repoUrl;
    installed.signers = signers;
    installed.path = path;
    installed.installedAt = Database::now();
    const auto language = contentLanguage();
    const bool anyInLanguage = std::any_of(package.sources.begin(), package.sources.end(),
                                           [&](const SourceInfo &s) { return inLanguage(s.lang, language); });
    Database::transaction([&] {
        Database::query(QStringLiteral("INSERT INTO ext_installed(kind, pkg, package, repo_url, signers, path, installed_at) "
                                       "VALUES(?, ?, ?, ?, ?, ?, ?) ON CONFLICT(kind, pkg) DO UPDATE SET package = excluded.package, "
                                       "repo_url = excluded.repo_url, signers = excluded.signers, path = excluded.path, "
                                       "installed_at = excluded.installed_at"),
                        {kindName(kind), pkg, compact(package.toJson()), repoUrl, signers.join(QLatin1Char(',')), path,
                         installed.installedAt});
        // New sources start on when they are in the content language (all
        // of them when none is); a source's switch is kept across updates.
        for (const auto &s : package.sources)
            Database::query(QStringLiteral("INSERT OR IGNORE INTO ext_sources(key, enabled) VALUES(?, ?)"),
                            {sourceKey(kind, s.id), !anyInLanguage || inLanguage(s.lang, language)});
    });
    writing([&](State &s) {
        s.installed.insert(id(kind, pkg), installed);
        for (const auto &source : package.sources) {
            const auto key = sourceKey(kind, source.id);
            if (!s.enabled.contains(key))
                s.enabled.insert(key, !anyInLanguage || inLanguage(source.lang, language));
        }
        installed.trusted = isTrusted(s, installed);
        return 0;
    });
    changed();
    return installed;
}

void Manager::uninstall(Kind kind, const QString &pkg) {
    const auto old = installedPackage(kind, pkg);
    Database::exec(QStringLiteral("DELETE FROM ext_installed WHERE kind = ? AND pkg = ?"), {kindName(kind), pkg});
    Database::exec(QStringLiteral("DELETE FROM ext_trust WHERE kind = ? AND pkg = ?"), {kindName(kind), pkg});
    if (old) {
        const auto folder = QFileInfo(old->path).absolutePath();
        for (const auto &file : QDir(folder).entryInfoList(QDir::Files))
            QFile::setPermissions(file.absoluteFilePath(), QFile::ReadOwner | QFile::WriteOwner);
        QDir(folder).removeRecursively();
    }
    writing([&](State &s) {
        s.installed.remove(id(kind, pkg));
        s.trust.remove(id(kind, pkg));
        return 0;
    });
    changed();
}

QList<Installed> Manager::installed(std::optional<Kind> kind) {
    const auto listed = available(kind);
    return reading([&](const State &s) {
        QList<Installed> out;
        for (auto i : s.installed) {
            if (kind && i.package.kind != *kind)
                continue;
            i.trusted = isTrusted(s, i);
            i.lib = libSupport(i.package.kind, i.package.lib);
            for (const auto &p : listed)
                if (p.kind == i.package.kind && p.pkg == i.package.pkg && p.versionCode > i.package.versionCode)
                    i.update = p;
            out.append(i);
        }
        std::sort(out.begin(), out.end(), [](const Installed &a, const Installed &b) {
            return a.package.name.compare(b.package.name, Qt::CaseInsensitive) < 0;
        });
        return out;
    });
}

std::optional<Installed> Manager::installedPackage(Kind kind, const QString &pkg) {
    return reading([&](const State &s) -> std::optional<Installed> {
        if (!s.installed.contains(id(kind, pkg)))
            return std::nullopt;
        auto i = s.installed.value(id(kind, pkg));
        i.trusted = isTrusted(s, i);
        i.lib = libSupport(i.package.kind, i.package.lib);
        return i;
    });
}

void Manager::trust(Kind kind, const QString &pkg) {
    const auto i = installedPackage(kind, pkg);
    if (!i || i->signers.isEmpty())
        return;
    Database::exec(QStringLiteral("INSERT INTO ext_trust(kind, pkg, version_code, signer) VALUES(?, ?, ?, ?) "
                                  "ON CONFLICT(kind, pkg) DO UPDATE SET version_code = excluded.version_code, signer = excluded.signer"),
                   {kindName(kind), pkg, i->package.versionCode, i->signers.last()});
    writing([&](State &s) {
        s.trust.insert(id(kind, pkg), {i->package.versionCode, i->signers.last()});
        return 0;
    });
    changed();
}

QList<Installed> Manager::updates() {
    QList<Installed> out;
    for (const auto &i : installed())
        if (i.update)
            out.append(i);
    return out;
}

QList<Installed> Manager::installTrustedUpdates() {
    QList<Installed> out;
    for (const auto &i : updates()) {
        if (!i.trusted)
            continue;
        try {
            auto next = install(i.package.kind, i.package.pkg);
            if (next.trusted)
                out.append(next);
        } catch (const Error &) {
            // Left for the Extensions page, which shows the update.
        }
    }
    return out;
}

// ---------------------------------------------------------------- sources

QList<SourceEntry> Manager::sources(std::optional<Kind> kind) {
    const auto all = installed(kind);
    return reading([&](const State &s) {
        QList<SourceEntry> out;
        for (const auto &i : all)
            for (const auto &source : i.package.sources) {
                SourceEntry e;
                e.kind = i.package.kind;
                e.id = source.id;
                e.key = sourceKey(e.kind, source.id);
                e.name = source.name;
                e.lang = source.lang;
                e.baseUrl = source.baseUrl;
                e.pkg = i.package.pkg;
                e.path = i.path;
                e.signer = i.signers.isEmpty() ? QString() : i.signers.last();
                e.enabled = s.enabled.value(e.key, true);
                e.nsfw = i.package.warning == ContentWarning::Nsfw;
                if (!i.trusted)
                    e.blocked = QStringLiteral("%1 is not trusted yet.").arg(i.package.name);
                else
                    e.blocked = libProblem(i.package);
                out.append(e);
            }
        return out;
    });
}

void Manager::setSourceEnabled(const QString &key, bool enabled) {
    Database::exec(QStringLiteral("INSERT INTO ext_sources(key, enabled) VALUES(?, ?) "
                                  "ON CONFLICT(key) DO UPDATE SET enabled = excluded.enabled"),
                   {key, enabled});
    writing([&](State &s) {
        s.enabled.insert(key, enabled);
        return 0;
    });
    changed();
}

void Manager::recordSourceResult(const QString &key, const QString &error) {
    // Written only when the health changes, not on every request.
    const bool same = reading([&](const State &s) { return s.errors.value(key) == error; });
    if (same)
        return;
    if (error.isEmpty())
        Database::exec(QStringLiteral("UPDATE ext_sources SET error = NULL, ok_at = ? WHERE key = ?"), {Database::now(), key});
    else
        Database::exec(QStringLiteral("INSERT INTO ext_sources(key, error, error_at) VALUES(?, ?, ?) "
                                      "ON CONFLICT(key) DO UPDATE SET error = excluded.error, error_at = excluded.error_at"),
                       {key, error, Database::now()});
    writing([&](State &s) {
        if (error.isEmpty())
            s.errors.remove(key);
        else
            s.errors.insert(key, error);
        return 0;
    });
}

QString Manager::sourceError(const QString &key) {
    return reading([&](const State &s) { return s.errors.value(key); });
}

QString Manager::preference(const QString &sourceKey, const QString &key) {
    return Database::scalar(QStringLiteral("SELECT value FROM ext_prefs WHERE source_key = ? AND key = ?"), {sourceKey, key}).toString();
}

void Manager::setPreference(const QString &sourceKey, const QString &key, const QString &value) {
    Database::exec(QStringLiteral("INSERT INTO ext_prefs(source_key, key, value) VALUES(?, ?, ?) "
                                  "ON CONFLICT(source_key, key) DO UPDATE SET value = excluded.value"),
                   {sourceKey, key, value});
}

} // namespace sc::ext
