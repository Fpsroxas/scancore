#include "Extensions.h"

#include "core/Http.h"
#include "sources/novels/LnCrawl.h"
#include "sources/novels/Registry.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QSysInfo>

namespace sc {

namespace {

constexpr const char *kUvVersion = "0.13.0";
constexpr const char *kPythonVersion = "3.13";
constexpr const char *kCrawlerVersion = "lightnovel-crawler==4.14.0";

QString uvAsset() {
#if defined(Q_OS_WIN)
    return QStringLiteral("uv-x86_64-pc-windows-msvc.zip");
#elif defined(Q_OS_MACOS)
    return QSysInfo::currentCpuArchitecture() == QLatin1String("arm64") ? QStringLiteral("uv-aarch64-apple-darwin.tar.gz")
                                                                         : QStringLiteral("uv-x86_64-apple-darwin.tar.gz");
#else
    return QStringLiteral("uv-x86_64-unknown-linux-gnu.tar.gz");
#endif
}

QString runTool(const QString &program, const QStringList &args, const QProcessEnvironment &env, int timeoutMs) {
    QProcess process;
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, args);
    if (!process.waitForStarted(15000))
        throw Error(QStringLiteral("Could not start %1.").arg(QFileInfo(program).fileName()));
    QElapsedTimer timer;
    timer.start();
    while (!process.waitForFinished(250)) {
        if (CancelScope::cancelled() || timer.elapsed() > timeoutMs) {
            process.kill();
            throw Error(CancelScope::cancelled() ? QStringLiteral("Cancelled") : QStringLiteral("The installation took too long."),
                        CancelScope::cancelled() ? 499 : 504);
        }
    }
    const auto output = QString::fromUtf8(process.readAll()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        throw Error(QStringLiteral("Installation step failed: %1").arg(output.section('\n', -1)));
    return output;
}

QString findFile(const QString &dir, const QString &name) {
    QDirIterator it(dir, {name}, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

} // namespace

bool Extensions::novelPackAvailable() const {
#ifdef SCANCORE_DISABLE_NOVEL_PACK
    return false;
#else
    return true;
#endif
}

bool Extensions::novelPackInstalled() const {
    return novelPackAvailable() && novels::LnCrawlRuntime::installed();
}

void Extensions::setStatus(const QString &status) {
    QMetaObject::invokeMethod(this, [this, status] {
        m_status = status;
        emit changed();
    });
}

Task *Extensions::installNovelPack() {
    if (!novelPackAvailable())
        return Task::run([]() -> QVariant { throw Error(QStringLiteral("The extended novel pack is disabled in this build.")); });
    m_busy = true;
    m_status = QStringLiteral("Starting…");
    emit changed();
    auto *task = Task::run([this]() -> QVariant {
        using novels::LnCrawlRuntime;
        const auto root = LnCrawlRuntime::root();
        QDir(root).removeRecursively();
        QDir().mkpath(root);

        setStatus(QStringLiteral("Downloading the installer…"));
        Request request;
        request.url = QUrl(QStringLiteral("https://github.com/astral-sh/uv/releases/download/%1/%2")
                               .arg(QString::fromLatin1(kUvVersion), uvAsset()));
        request.timeoutMs = 120000;
        request.maxBytes = 80 * 1024 * 1024;
        const auto archive = Http::send(request);
        if (!archive.ok())
            throw Error(QStringLiteral("Could not download the installer (HTTP %1).").arg(archive.status));
        const auto archivePath = root + QLatin1Char('/') + uvAsset();
        QSaveFile out(archivePath);
        if (!out.open(QIODevice::WriteOnly) || out.write(archive.body) != archive.body.size() || !out.commit())
            throw Error(QStringLiteral("Could not write to the app data folder."));
        const auto uvDir = root + QStringLiteral("/uv");
        QDir().mkpath(uvDir);
        runTool(QStringLiteral("tar"), {QStringLiteral("-xf"), archivePath, QStringLiteral("-C"), uvDir},
                QProcessEnvironment::systemEnvironment(), 120000);
#ifdef Q_OS_WIN
        const auto uv = findFile(uvDir, QStringLiteral("uv.exe"));
#else
        const auto uv = findFile(uvDir, QStringLiteral("uv"));
#endif
        if (uv.isEmpty())
            throw Error(QStringLiteral("The installer archive was not as expected."));
        QFile::setPermissions(uv, QFile::permissions(uv) | QFileDevice::ExeOwner);

        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("UV_PYTHON_INSTALL_DIR"), root + QStringLiteral("/python"));
        env.insert(QStringLiteral("UV_CACHE_DIR"), root + QStringLiteral("/cache"));
        env.insert(QStringLiteral("UV_NO_CONFIG"), QStringLiteral("1"));
        env.insert(QStringLiteral("UV_PYTHON_PREFERENCE"), QStringLiteral("only-managed"));

        setStatus(QStringLiteral("Installing Python…"));
        runTool(uv, {QStringLiteral("python"), QStringLiteral("install"), QString::fromLatin1(kPythonVersion)}, env, 600000);
        const auto python =
            runTool(uv, {QStringLiteral("python"), QStringLiteral("find"), QString::fromLatin1(kPythonVersion)}, env, 60000)
                .section('\n', -1)
                .trimmed();
        if (python.isEmpty() || !QFile::exists(python))
            throw Error(QStringLiteral("Python was not installed correctly."));

        setStatus(QStringLiteral("Installing the novel sources…"));
        runTool(uv,
                {QStringLiteral("pip"), QStringLiteral("install"), QStringLiteral("--python"), python, QStringLiteral("--target"),
                 LnCrawlRuntime::vendor(), QString::fromLatin1(kCrawlerVersion)},
                env, 900000);

        QFile::remove(LnCrawlRuntime::bridge());
        if (!QFile::copy(QStringLiteral(":/lncrawl/bridge.py"), LnCrawlRuntime::bridge()))
            throw Error(QStringLiteral("Could not write the source bridge."));
        QFile::setPermissions(LnCrawlRuntime::bridge(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);

        setStatus(QStringLiteral("Checking the installation…"));
        auto check = QProcessEnvironment::systemEnvironment();
        check.insert(QStringLiteral("PYTHONPATH"), LnCrawlRuntime::vendor());
        runTool(python, {QStringLiteral("-s"), QStringLiteral("-c"), QStringLiteral("import lncrawl.core, scraper")}, check, 120000);

        QSaveFile marker(root + QStringLiteral("/python-path.txt"));
        if (!marker.open(QIODevice::WriteOnly) || marker.write(python.toUtf8()) < 0 || !marker.commit())
            throw Error(QStringLiteral("Could not write to the app data folder."));
        QDir(root + QStringLiteral("/cache")).removeRecursively();
        QDir(uvDir).removeRecursively();
        QFile::remove(archivePath);
        novels::Registry::rebuild();
        return QVariantMap{{"sources", novels::Registry::all().size()}};
    });
    connect(task, &Task::changed, this, [this, task] {
        if (task->loading())
            return;
        m_busy = false;
        m_status = task->ok() ? QString() : task->error();
        emit changed();
    });
    return task;
}

void Extensions::removeNovelPack() {
    if (!novelPackAvailable())
        return;
    QDir(novels::LnCrawlRuntime::root()).removeRecursively();
    novels::Registry::rebuild();
    emit changed();
}

} // namespace sc
