// Saving anime episodes for offline playback: HLS variants, separate audio,
// keys, init sections and byte ranges, against a fixture server.
#include "core/Http.h"
#include "core/VideoSave.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace sc;

class Video : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    QHash<QString, int> m_hits;

    static QString read(const QString &path) {
        QFile file(path);
        file.open(QIODevice::ReadOnly);
        return QString::fromUtf8(file.readAll());
    }

private slots:
    void initTestCase() {
        Http::setTestHandler([this](const Request &request) -> std::optional<Response> {
            if (request.url.host() != QLatin1String("video.test"))
                return std::nullopt;
            const auto path = request.url.path();
            ++m_hits[path];
            Response r;
            r.status = 200;
            r.finalUrl = request.url;
            // The master redirects to a CDN folder; parts are relative to it.
            if (path == QLatin1String("/watch/master.m3u8")) {
                r.finalUrl = QUrl(QStringLiteral("https://video.test/cdn/master.m3u8"));
                r.body = "#EXTM3U\n"
                         "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aud\",NAME=\"Japanese\",DEFAULT=YES,URI=\"audio/ja.m3u8\"\n"
                         "#EXT-X-STREAM-INF:BANDWIDTH=900000,RESOLUTION=854x480,AUDIO=\"aud\"\nlow/index.m3u8\n"
                         "#EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080,AUDIO=\"aud\"\nhigh/index.m3u8\n"
                         "#EXT-X-STREAM-INF:BANDWIDTH=9000000,RESOLUTION=3840x2160,AUDIO=\"aud\"\nuhd/index.m3u8\n";
            } else if (path == QLatin1String("/cdn/high/index.m3u8")) {
                r.body = "#EXTM3U\n#EXT-X-TARGETDURATION:4\n"
                         "#EXT-X-KEY:METHOD=AES-128,URI=\"https://video.test/keys/k1\",IV=0x01\n"
                         "#EXTINF:4,\nseg1.jpg\n#EXTINF:4,\nseg2.jpg\n#EXT-X-ENDLIST\n";
            } else if (path == QLatin1String("/cdn/audio/ja.m3u8")) {
                r.body = "#EXTM3U\n#EXT-X-MAP:URI=\"init.mp4\"\n#EXTINF:4,\n#EXT-X-BYTERANGE:100@0\nall.m4s\n"
                         "#EXTINF:4,\n#EXT-X-BYTERANGE:50\nall.m4s\n#EXT-X-ENDLIST\n";
            } else if (path == QLatin1String("/keys/k1")) {
                r.body = QByteArray(16, 'k');
            } else if (path.endsWith(QLatin1String(".jpg"))) {
                // Transport stream parts under another name.
                r.body = QByteArray(1, char(0x47)) + path.toUtf8();
            } else if (path == QLatin1String("/cdn/audio/init.mp4")) {
                r.body = QByteArray("\0\0\0\x18" "ftypiso6", 12);
            } else if (path == QLatin1String("/cdn/audio/all.m4s")) {
                QByteArray range;
                for (const auto &[name, value] : request.headers)
                    if (name == "Range")
                        range = value;
                r.status = 206;
                r.body = QByteArray("\0\0\0\x10" "moof", 8) + range;
            } else {
                r.status = 404;
            }
            return r;
        });
    }

    void picksVariantAtMost1080() {
        const auto v = video::pickVariant(QStringLiteral("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1,RESOLUTION=1x2160\nuhd.m3u8\n"
                                                         "#EXT-X-STREAM-INF:BANDWIDTH=2,RESOLUTION=1x720,AUDIO=\"a\"\nhd.m3u8\n"));
        QCOMPARE(v.uri, QStringLiteral("hd.m3u8"));
        QCOMPARE(v.audioGroup, QStringLiteral("a"));
        // Only taller ones: the tallest.
        QCOMPARE(video::pickVariant(QStringLiteral("#EXT-X-STREAM-INF:RESOLUTION=1x2160\nuhd.m3u8\n")).uri, QStringLiteral("uhd.m3u8"));
    }

    void savesHlsWithAudioKeysAndRanges() {
        const auto folder = m_dir.filePath(QStringLiteral("ep1"));
        qint64 lastDone = 0, lastTotal = 0;
        const auto saved = video::save(QUrl(QStringLiteral("https://video.test/watch/master.m3u8")), {}, folder,
                                       [&](qint64 done, qint64 total) { lastDone = done, lastTotal = total; });
        QVERIFY(saved.hls);
        QCOMPARE(saved.file, QStringLiteral("video.m3u8"));
        QCOMPARE(lastDone, 4);
        QCOMPARE(lastTotal, 4);
        const auto master = read(folder + QStringLiteral("/video.m3u8"));
        QVERIFY(master.contains(QStringLiteral("URI=\"a.m3u8\"")));
        QVERIFY(master.contains(QStringLiteral("\nv.m3u8\n")));
        const auto video = read(folder + QStringLiteral("/v.m3u8"));
        QVERIFY(video.contains(QStringLiteral("URI=\"v-key.1.key\",IV=0x01")));
        QVERIFY(video.contains(QStringLiteral("\nv-00001.ts\n")));
        QVERIFY(video.contains(QStringLiteral("\nv-00002.ts\n")));
        QCOMPARE(read(folder + QStringLiteral("/v-00002.ts")).mid(1), QStringLiteral("/cdn/high/seg2.jpg"));
        QCOMPARE(m_hits.value(QStringLiteral("/cdn/uhd/index.m3u8")), 0);
        const auto audio = read(folder + QStringLiteral("/a.m3u8"));
        QVERIFY(audio.contains(QStringLiteral("#EXT-X-MAP:URI=\"a-init.1.mp4\"")));
        QVERIFY(!audio.contains(QStringLiteral("BYTERANGE")));
        QVERIFY(audio.contains(QStringLiteral("\na-00001.m4s\n")));
        QVERIFY(read(folder + QStringLiteral("/a-00001.m4s")).endsWith(QStringLiteral("0-99")));
        QVERIFY(read(folder + QStringLiteral("/a-00002.m4s")).endsWith(QStringLiteral("100-149")));

        // Again: what is there is kept, only the playlists are asked again.
        m_hits.clear();
        video::save(QUrl(QStringLiteral("https://video.test/watch/master.m3u8")), {}, folder, [](qint64, qint64) {});
        QCOMPARE(m_hits.value(QStringLiteral("/cdn/high/seg1.jpg")), 0);
        QCOMPARE(m_hits.value(QStringLiteral("/keys/k1")), 0);
        QCOMPARE(m_hits.value(QStringLiteral("/cdn/high/index.m3u8")), 1);
    }

    // Against a real server: SCANCORE_VIDEO_URL (HLS or a file) is saved
    // into SCANCORE_VIDEO_DIR; skipped without them.
    void savesFromNetwork() {
        const auto url = qEnvironmentVariable("SCANCORE_VIDEO_URL");
        if (url.isEmpty())
            QSKIP("SCANCORE_VIDEO_URL is not set");
        const auto saved = video::save(QUrl(url), {}, qEnvironmentVariable("SCANCORE_VIDEO_DIR"), [](qint64, qint64) {});
        qInfo("saved %s (hls %d)", qPrintable(saved.file), saved.hls);
    }

    void failsOnMissingPart() {
        bool failed = false;
        try {
            video::save(QUrl(QStringLiteral("https://video.test/missing.m3u8")), {}, m_dir.filePath(QStringLiteral("x")), [](qint64, qint64) {});
        } catch (const Error &) {
            failed = true;
        }
        QVERIFY(failed);
    }
};

QTEST_MAIN(Video)
#include "tst_video.moc"
