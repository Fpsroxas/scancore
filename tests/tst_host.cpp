// The extension host from the app's side, end to end and offline: a
// fixture repository on repo.test lists the test extension (host/testext,
// signed by its build), the app installs and trusts it, the registry makes
// it a source, the host runs as a helper process, and the extension reads a
// fixture site on fixture.test served from this test.

#include "core/Database.h"
#include "core/Http.h"
#include "core/ImageCache.h"
#include "core/Parallel.h"
#include "extensions/ExtensionSource.h"
#include "extensions/Manager.h"
#include "extensions/ProcessHost.h"
#include "extensions/Runtime.h"
#include "sources/comics/Registry.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

using namespace sc;

namespace {

QByteArray read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QByteArray site(const QString &name) { return read(QStringLiteral(SCANCORE_SOURCE_DIR "/tests/fixtures/host/") + name); }

// Mihon's source id: the first 8 bytes of MD5("<name>/<lang>/<versionId>"),
// big-endian, without the sign bit.
QString mihonId(const QString &name, const QString &lang) {
    const auto md5 = QCryptographicHash::hash(QStringLiteral("%1/%2/1").arg(name.toLower(), lang).toUtf8(), QCryptographicHash::Md5);
    quint64 id = 0;
    for (int i = 0; i < 8; ++i)
        id = (id << 8) | uchar(md5[i]);
    return QString::number(qint64(id & 0x7fffffffffffffffULL));
}

// The fixture site, on its own thread so it answers while the test waits on
// the host.
class FixtureSite : public QObject {
public:
    FixtureSite() {
        moveToThread(&m_thread);
        m_thread.start();
        QMetaObject::invokeMethod(this, [this] { listen(); }, Qt::BlockingQueuedConnection);
    }
    ~FixtureSite() override {
        QMetaObject::invokeMethod(this, [this] { delete m_server; }, Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
    }
    quint16 port() const { return m_port; }
    QStringList requests() {
        QMutexLocker lock(&m_mutex);
        return m_requests;
    }

private:
    void listen() {
        m_server = new QTcpServer;
        m_server->listen(QHostAddress::LocalHost);
        m_port = m_server->serverPort();
        QObject::connect(m_server, &QTcpServer::newConnection, m_server, [this] {
            while (auto *socket = m_server->nextPendingConnection()) {
                auto *buffer = new QByteArray;
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
                    *buffer += socket->readAll();
                    if (!buffer->contains("\r\n\r\n"))
                        return;
                    answer(socket, *buffer);
                    delete buffer;
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    void answer(QTcpSocket *socket, const QByteArray &request) {
        const auto lines = request.split('\n');
        const auto target = lines.value(0).split(' ').value(1);
        const auto path = target.split('?').value(0);
        const bool ownClient = request.contains("X-Test-Source: 1") || request.contains("x-test-source: 1");
        {
            QMutexLocker lock(&m_mutex);
            m_requests.append(QString::fromUtf8(target));
        }
        QByteArray body;
        QByteArray type = "application/json";
        if (path == "/popular")
            body = site(QStringLiteral("popular.json"));
        else if (path == "/search")
            body = site(QStringLiteral("search.json"));
        else if (path == "/series/1")
            body = site(QStringLiteral("series-1.json"));
        else if (path == "/series/1/chapters")
            body = site(QStringLiteral("chapters.json"));
        else if (path == "/chapter/2/pages")
            body = site(QStringLiteral("pages.json"));
        else if (path == "/page/2")
            body = site(QStringLiteral("page-2.json"));
        else if (path.startsWith("/img/") && ownClient) {
            body = site(QStringLiteral("image.png"));
            type = "image/png";
        }
        const QByteArray status = body.isEmpty() ? "404 Not Found" : "200 OK";
        socket->write("HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size()) +
                      "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    QThread m_thread;
    QTcpServer *m_server = nullptr;
    quint16 m_port = 0;
    QMutex m_mutex;
    QStringList m_requests;
};

} // namespace

class HostTests : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    std::unique_ptr<FixtureSite> m_site;
    std::shared_ptr<ext::ProcessHost> m_host;
    QString m_sourceKey;

private slots:
    void initTestCase() {
        const QString apk = QStringLiteral(SCANCORE_TESTEXT_APK);
        if (!QFileInfo::exists(apk))
            QSKIP("No test extension APK (the host's build found no Android SDK).");
        auto launch = ext::ProcessHost::find({}, QStringLiteral(SCANCORE_HOST_DEV_LIB));
        if (launch.java.isEmpty())
            QSKIP("No Java to run the extension host.");
        QVERIFY(m_dir.isValid());
        Database::open(m_dir.filePath(QStringLiteral("test.db")));
        ext::Manager::setStorage(m_dir.filePath(QStringLiteral("extensions")));
        ImageCache::init(m_dir.filePath(QStringLiteral("images")), 64 * 1024 * 1024);
        ext::ExtensionSource::installImageWriter();

        m_site = std::make_unique<FixtureSite>();
        qputenv("SCANCORE_HOST_TEST_LOOPBACK", "1");
        launch.root = m_dir.filePath(QStringLiteral("host"));
        launch.jvmArgs << QStringLiteral("-Dscancore.testext.base=http://fixture.test:%1").arg(m_site->port());
        m_host = std::make_shared<ext::ProcessHost>(launch);
        ext::setHost(ext::Kind::Mihon, m_host);

        // The repository, signed by the key the test extension was signed with.
        const auto signer = QString::fromLatin1(read(QFileInfo(apk).absolutePath() + QStringLiteral("/signer.txt"))).trimmed();
        const auto id = mihonId(QStringLiteral("Test Source"), QStringLiteral("en"));
        m_sourceKey = QStringLiteral("mihon:") + id;
        const auto index = QJsonDocument(QJsonArray{QJsonObject{
                                             {"name", "Tachiyomi: Test Source"},
                                             {"pkg", "com.mediumdev.scancore.testext"},
                                             {"apk", "testext.apk"},
                                             {"lang", "en"},
                                             {"code", 3},
                                             {"version", "1.4.3"},
                                             {"nsfw", 0},
                                             {"sources", QJsonArray{QJsonObject{{"name", "Test Source"}, {"lang", "en"}, {"id", id}, {"baseUrl", "http://fixture.test"}}}}}})
                               .toJson();
        const auto repo = QJsonDocument(QJsonObject{{"meta", QJsonObject{{"name", "Test Repository"}, {"website", "https://repo.test"},
                                                                          {"signingKeyFingerprint", signer}}}})
                              .toJson();
        const auto apkBytes = read(apk);
        Http::setTestHandler([=](const Request &request) -> std::optional<Response> {
            if (request.url.host() != QLatin1String("repo.test"))
                return std::nullopt;
            Response r;
            r.status = 200;
            const auto path = request.url.path();
            r.body = path.endsWith(QLatin1String("index.min.json")) ? index
                     : path.endsWith(QLatin1String("repo.json"))      ? repo
                     : path.endsWith(QLatin1String(".apk"))           ? apkBytes
                                                                      : QByteArray();
            if (r.body.isEmpty())
                r.status = 404;
            return r;
        });
    }

    void cleanupTestCase() {
        if (m_host)
            m_host->stop();
        Http::setTestHandler({});
    }

    void installAndTrust() {
        const auto preview = ext::Manager::preview(QStringLiteral("https://repo.test/repo/index.min.json"));
        QCOMPARE(preview.packages.size(), 1);
        ext::Manager::add(preview);
        const auto installed = ext::Manager::install(ext::Kind::Mihon, QStringLiteral("com.mediumdev.scancore.testext"));
        // Signed with the repository's key.
        QVERIFY(installed.trusted);
        comics::Registry::rebuild();
        QVERIFY(comics::Registry::byKey(m_sourceKey));
    }

    void browseThroughTheHost() {
        auto source = comics::Registry::byKey(m_sourceKey);
        QVERIFY(source);
        QElapsedTimer timer;
        timer.start();
        const auto popular = source->browse(QStringLiteral("popular"), 1, {}, {});
        const auto cold = timer.elapsed();
        QCOMPARE(popular.items.size(), 2);
        QVERIFY(popular.hasNextPage);
        QCOMPARE(popular.items[0].url, m_sourceKey + QStringLiteral(":/series/1"));

        timer.restart();
        const auto found = source->search(QStringLiteral("one"));
        const auto warm = timer.elapsed();
        QCOMPARE(found.size(), 1);
        QVERIFY(m_site->requests().last().contains(QLatin1String("q=one")));
        qInfo("first call (host start, verify, convert, load): %lld ms; next call: %lld ms", cold, warm);
        QVERIFY2(warm < 1000, qPrintable(QStringLiteral("a warm call took %1 ms").arg(warm)));

        const auto details = source->details(found[0].url);
        QCOMPARE(details->author, QStringLiteral("A. Writer"));
        QCOMPARE(details->status, QStringLiteral("Ongoing"));

        const auto chapters = source->chapters(found[0].url);
        QCOMPARE(chapters.size(), 2);
        QCOMPARE(chapters[0].number, QStringLiteral("1"));
        QCOMPARE(chapters[1].url, m_sourceKey + QStringLiteral(":/chapter/2"));

        // Pages come back as extimage: panels; the image cache has the host
        // fetch them through the extension's own client into the cache.
        const auto panels = source->panels(chapters[1].url);
        QCOMPARE(panels.size(), 2);
        const auto image = ImageCache::fetch(panels[1].url, {});
        QCOMPARE(image, site(QStringLiteral("image.png")));
        // A second read comes from the cache, not the site.
        const auto before = m_site->requests().size();
        QCOMPARE(ImageCache::fetch(panels[1].url, {}), image);
        QCOMPARE(m_site->requests().size(), before);
    }

    void manyCallsAtOnce() {
        auto source = comics::Registry::byKey(m_sourceKey);
        QList<std::function<int()>> jobs;
        for (int i = 0; i < 16; ++i)
            jobs.append([source] { return int(source->browse(QStringLiteral("popular"), 1, {}, {}).items.size()); });
        QElapsedTimer timer;
        timer.start();
        const auto results = parallel(jobs);
        for (const auto &r : results)
            QVERIFY2(r.ok() && *r.value == 2, qPrintable(r.error));
        qInfo("16 calls at once: %lld ms", timer.elapsed());
    }

    void errorsAndRestart() {
        auto source = comics::Registry::byKey(m_sourceKey);
        // The site has no series 9: the extension's error reaches the app
        // as a message, not a crash.
        try {
            source->chapters(m_sourceKey + QStringLiteral(":/series/9"));
            QFAIL("listed chapters of a missing series");
        } catch (const Error &e) {
            QVERIFY(!e.message().isEmpty());
            QVERIFY(!ext::Manager::sourceError(m_sourceKey).isEmpty());
        }
        // A stopped host starts again on the next call.
        m_host->stop();
        QCOMPARE(source->search(QStringLiteral("one")).size(), 1);
        QVERIFY(ext::Manager::sourceError(m_sourceKey).isEmpty());
    }

    // The runtime zip the release workflow publishes (when it was built
    // here with ./gradlew :desktop:runtimeZip): installed the way the app
    // does, then running the host.
    void runtimeZipRuns() {
        if (ext::Runtime::platform().isEmpty())
            QSKIP("No Extensions runtime for this computer.");
        const auto zipPath = QStringLiteral(SCANCORE_SOURCE_DIR "/host/desktop/build/distributions/SCANCORE-extensions-runtime-%1-%2.zip")
                                 .arg(QLatin1String(ext::Runtime::HostVersion), ext::Runtime::platform());
        if (!QFileInfo::exists(zipPath))
            QSKIP("No runtime zip built (./gradlew :desktop:runtimeZip in host/).");
        const auto zip = read(zipPath);
        const auto sum = read(zipPath + QStringLiteral(".sha256"));
        ext::Runtime::setDir(m_dir.filePath(QStringLiteral("runtime")));
        ext::Runtime::setDownloadBase(QStringLiteral("https://runtime.test"));
        Http::setTestHandler([&](const Request &request) -> std::optional<Response> {
            if (request.url.host() != QLatin1String("runtime.test"))
                return std::nullopt;
            Response r;
            r.status = 200;
            r.body = request.url.path().endsWith(QLatin1String(".sha256")) ? sum : zip;
            return r;
        });
        QElapsedTimer timer;
        timer.start();
        ext::Runtime::install();
        qInfo("installing the %lld MB runtime: %lld ms", qint64(zip.size() / 1024 / 1024), timer.elapsed());
        QVERIFY(ext::Runtime::installed());
        auto launch = ext::ProcessHost::find(ext::Runtime::dir(), {});
        QVERIFY(launch.java.startsWith(ext::Runtime::dir()));
        launch.root = m_dir.filePath(QStringLiteral("runtime-host"));
        ext::ProcessHost host(launch);
        timer.restart();
        const auto hello = host.call(QStringLiteral("hello"), {}).toObject();
        qInfo("host start on the runtime: %lld ms", timer.elapsed());
        QCOMPARE(hello.value("version").toString(), QString::fromLatin1(ext::Runtime::HostVersion));
        host.stop();
    }

    void cancelledCallsReturnAtOnce() {
        auto source = comics::Registry::byKey(m_sourceKey);
        auto flag = std::make_shared<std::atomic_bool>(true);
        CancelScope scope(flag);
        QElapsedTimer timer;
        timer.start();
        try {
            source->search(QStringLiteral("one"));
        } catch (const Error &e) {
            QCOMPARE(e.status(), 499);
        }
        QVERIFY(timer.elapsed() < 500);
    }
};

QTEST_GUILESS_MAIN(HostTests)
#include "tst_host.moc"
