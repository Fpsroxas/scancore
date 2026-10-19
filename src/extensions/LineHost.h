#pragma once

#include "Host.h"

#include <QByteArray>
#include <QHash>

#include <atomic>
#include <memory>
#include <mutex>

namespace sc::ext {

// A host spoken to with one JSON object per line (see host/core/Rpc.kt):
// requests carry an id, answers come back in any order and are matched to
// the call waiting for them. Calls block their worker thread; a cancelled
// one returns at once and tells the host to stop working on it. Subclasses
// say how a line gets there: a helper process's stdin (desktop) or JNI
// (Android).
class LineHost : public Host {
public:
    QJsonValue call(const QString &method, const QJsonObject &params) override;

protected:
    // One request line, newline included; from any thread.
    virtual void sendLine(const QByteArray &line) = 0;
    // An answer line from the host.
    void receive(const QByteArray &line);
    // The host went away: every waiting call fails with `message`.
    void failAll(const QString &message);
    bool busy() const;

private:
    struct Waiting;
    mutable std::mutex m_mutex;
    QHash<qint64, std::shared_ptr<Waiting>> m_pending;
    std::atomic<qint64> m_nextId{1};
};

} // namespace sc::ext
