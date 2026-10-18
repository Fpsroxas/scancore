#include "Database.h"

#include "Http.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QRecursiveMutex>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>
#include <QUuid>

namespace sc {

namespace {

QString g_path;
QRecursiveMutex g_writeLock;

// Schema migrations, applied in order. Never edit a shipped step; append.
const char *const kMigrations[] = {
    R"(
    CREATE TABLE series (
        id TEXT PRIMARY KEY,
        media TEXT NOT NULL,
        source TEXT NOT NULL,
        source_url TEXT NOT NULL,
        title TEXT NOT NULL,
        cover TEXT,
        description TEXT,
        author TEXT,
        status TEXT,
        genres TEXT,
        extra TEXT,
        updated_at INTEGER
    );
    CREATE TABLE units (
        series_id TEXT NOT NULL,
        unit_key TEXT NOT NULL,
        idx INTEGER NOT NULL,
        number TEXT,
        title TEXT,
        url TEXT,
        released_at TEXT,
        PRIMARY KEY (series_id, unit_key)
    );
    CREATE INDEX units_series ON units(series_id, idx);
    CREATE TABLE library (
        series_id TEXT PRIMARY KEY,
        category TEXT NOT NULL DEFAULT 'reading',
        added_at INTEGER NOT NULL,
        last_checked INTEGER,
        known_units INTEGER NOT NULL DEFAULT 0,
        new_units INTEGER NOT NULL DEFAULT 0,
        latest_title TEXT
    );
    CREATE TABLE progress (
        series_id TEXT NOT NULL,
        unit_key TEXT NOT NULL,
        unit_title TEXT,
        unit_number TEXT,
        unit_url TEXT,
        position TEXT,
        percent REAL NOT NULL DEFAULT 0,
        completed INTEGER NOT NULL DEFAULT 0,
        updated_at INTEGER NOT NULL,
        PRIMARY KEY (series_id, unit_key)
    );
    CREATE INDEX progress_recent ON progress(updated_at DESC);
    CREATE TABLE prefs (key TEXT PRIMARY KEY, value TEXT);
    CREATE TABLE cache (key TEXT PRIMARY KEY, value BLOB, expires INTEGER NOT NULL);
    CREATE TABLE downloads (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        series_id TEXT NOT NULL,
        unit_key TEXT NOT NULL,
        unit_title TEXT,
        unit_url TEXT,
        status TEXT NOT NULL,
        done INTEGER NOT NULL DEFAULT 0,
        total INTEGER NOT NULL DEFAULT 0,
        error TEXT,
        path TEXT,
        created_at INTEGER NOT NULL,
        UNIQUE (series_id, unit_key)
    );
    )",
    // 2: activity per day (time and finished chapters or episodes, per
    // device so synced devices add up), and what file sync needs to merge
    // libraries: when an entry last changed, and removals.
    R"(
    CREATE TABLE activity (
        day TEXT NOT NULL,
        device TEXT NOT NULL,
        media TEXT NOT NULL,
        seconds INTEGER NOT NULL DEFAULT 0,
        units INTEGER NOT NULL DEFAULT 0,
        PRIMARY KEY (day, device, media)
    );
    ALTER TABLE library ADD COLUMN changed_at INTEGER;
    UPDATE library SET changed_at = added_at;
    CREATE TABLE library_removed (series_id TEXT PRIMARY KEY, removed_at INTEGER NOT NULL);
    )",
};

QSqlDatabase connection() {
    const auto name = QStringLiteral("sc-%1").arg(quintptr(QThread::currentThreadId()));
    if (QSqlDatabase::contains(name)) {
        auto db = QSqlDatabase::database(name, false);
        if (db.isOpen())
            return db;
    }
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(g_path);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!db.open())
        throw Error(QStringLiteral("Could not open the library database: %1").arg(db.lastError().text()));
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    return db;
}

QSqlQuery run(const QString &sql, const QVariantList &binds) {
    auto db = connection();
    QSqlQuery query(db);
    query.setForwardOnly(true);
    if (!query.prepare(sql))
        throw Error(QStringLiteral("Database error: %1").arg(query.lastError().text()));
    for (const auto &value : binds)
        query.addBindValue(value);
    if (!query.exec())
        throw Error(QStringLiteral("Database error: %1").arg(query.lastError().text()));
    return query;
}

bool isWrite(const QString &sql) {
    const auto head = sql.trimmed().left(8).toUpper();
    return !head.startsWith(QLatin1String("SELECT")) && !head.startsWith(QLatin1String("WITH"));
}

} // namespace

void Database::open(const QString &path) {
    g_path = path;
    auto db = connection();
    QSqlQuery query(db);
    query.exec(QStringLiteral("PRAGMA user_version"));
    int version = query.next() ? query.value(0).toInt() : 0;
    const int target = int(std::size(kMigrations));
    for (; version < target; ++version) {
        db.transaction();
        for (const auto &statement : QString::fromUtf8(kMigrations[version]).split(QLatin1Char(';'))) {
            if (statement.trimmed().isEmpty())
                continue;
            QSqlQuery step(db);
            if (!step.exec(statement)) {
                db.rollback();
                throw Error(QStringLiteral("Database upgrade failed: %1").arg(step.lastError().text()));
            }
        }
        QSqlQuery(db).exec(QStringLiteral("PRAGMA user_version = %1").arg(version + 1));
        db.commit();
    }
}

QString Database::path() { return g_path; }

QVariantList Database::query(const QString &sql, const QVariantList &binds) {
    std::unique_ptr<QMutexLocker<QRecursiveMutex>> lock;
    if (isWrite(sql))
        lock = std::make_unique<QMutexLocker<QRecursiveMutex>>(&g_writeLock);
    auto q = run(sql, binds);
    QVariantList rows;
    if (!q.isSelect())
        return rows;
    const auto record = q.record();
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < record.count(); ++i)
            row.insert(record.fieldName(i), q.value(i));
        rows.append(row);
    }
    return rows;
}

QVariantMap Database::one(const QString &sql, const QVariantList &binds) {
    const auto rows = query(sql, binds);
    return rows.isEmpty() ? QVariantMap() : rows.first().toMap();
}

QVariant Database::scalar(const QString &sql, const QVariantList &binds) {
    auto q = run(sql, binds);
    return q.next() ? q.value(0) : QVariant();
}

void Database::exec(const QString &sql, const QVariantList &binds) {
    QMutexLocker lock(&g_writeLock);
    run(sql, binds);
}

void Database::transaction(const std::function<void()> &work) {
    QMutexLocker lock(&g_writeLock);
    auto db = connection();
    db.transaction();
    try {
        work();
        db.commit();
    } catch (...) {
        db.rollback();
        throw;
    }
}

std::optional<QByteArray> Database::cacheGet(const QString &key, bool allowStale) {
    auto q = run(QStringLiteral("SELECT value, expires FROM cache WHERE key = ?"), {key});
    if (!q.next())
        return std::nullopt;
    if (!allowStale && q.value(1).toLongLong() < now())
        return std::nullopt;
    return q.value(0).toByteArray();
}

void Database::cachePut(const QString &key, const QByteArray &value, qint64 ttlSeconds) {
    QMutexLocker lock(&g_writeLock);
    run(QStringLiteral("INSERT INTO cache(key, value, expires) VALUES(?, ?, ?) "
                       "ON CONFLICT(key) DO UPDATE SET value = excluded.value, expires = excluded.expires"),
        {key, value, now() + ttlSeconds});
}

void Database::cachePrune() {
    // Keep expired rows for a week as stale fallbacks when a site is down.
    exec(QStringLiteral("DELETE FROM cache WHERE expires < ?"), {now() - 7 * 86400});
}

QVariant Database::pref(const QString &key, const QVariant &fallback) {
    auto q = run(QStringLiteral("SELECT value FROM prefs WHERE key = ?"), {key});
    if (!q.next())
        return fallback;
    const auto doc = QJsonDocument::fromJson(q.value(0).toByteArray());
    if (doc.isNull())
        return fallback;
    return doc.object().value(QStringLiteral("v")).toVariant();
}

void Database::setPref(const QString &key, const QVariant &value) {
    QJsonObject wrapper{{"v", QJsonValue::fromVariant(value)}};
    exec(QStringLiteral("INSERT INTO prefs(key, value) VALUES(?, ?) "
                        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"),
         {key, QJsonDocument(wrapper).toJson(QJsonDocument::Compact)});
}

qint64 Database::now() { return QDateTime::currentSecsSinceEpoch(); }

} // namespace sc
