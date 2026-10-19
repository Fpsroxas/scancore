#include "LineHost.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <condition_variable>

namespace sc::ext {

struct LineHost::Waiting {
    std::mutex mutex;
    std::condition_variable arrived;
    bool done = false;
    QJsonObject reply;
};

QJsonValue LineHost::call(const QString &method, const QJsonObject &params) {
    if (!available())
        throw Error(unavailableReason());
    const auto id = m_nextId++;
    auto waiting = std::make_shared<Waiting>();
    {
        std::lock_guard lock(m_mutex);
        m_pending.insert(id, waiting);
    }
    sendLine(QJsonDocument(QJsonObject{{"id", id}, {"method", method}, {"params", params}}).toJson(QJsonDocument::Compact) + '\n');

    const auto giveUp = [&](const Error &error) {
        {
            std::lock_guard lock(m_mutex);
            m_pending.remove(id);
        }
        // The host stops the call; its answer, if any, is dropped.
        sendLine(QJsonDocument(QJsonObject{{"id", 0}, {"method", "cancel"}, {"params", QJsonObject{{"id", id}}}})
                     .toJson(QJsonDocument::Compact) +
                 '\n');
        throw error;
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(135);
    std::unique_lock lock(waiting->mutex);
    while (!waiting->done) {
        if (CancelScope::cancelled()) {
            lock.unlock();
            giveUp(Error(QStringLiteral("Cancelled"), 499));
        }
        if (std::chrono::steady_clock::now() > deadline) {
            lock.unlock();
            giveUp(Error(QStringLiteral("The extension took too long to answer."), 504));
        }
        waiting->arrived.wait_for(lock, std::chrono::milliseconds(50));
    }
    const auto reply = waiting->reply;
    lock.unlock();
    if (reply.contains(QStringLiteral("error"))) {
        const auto e = reply.value(QStringLiteral("error")).toObject();
        throw Error(e.value(QStringLiteral("message")).toString(QStringLiteral("The extension failed.")),
                    e.value(QStringLiteral("status")).toInt(), e.value(QStringLiteral("verifyUrl")).toString());
    }
    return reply.value(QStringLiteral("result"));
}

void LineHost::receive(const QByteArray &line) {
    const auto reply = QJsonDocument::fromJson(line).object();
    if (!reply.contains(QStringLiteral("id")))
        return;
    std::shared_ptr<Waiting> waiting;
    {
        std::lock_guard lock(m_mutex);
        waiting = m_pending.take(reply.value(QStringLiteral("id")).toInteger());
    }
    if (!waiting)
        return;
    {
        std::lock_guard lock(waiting->mutex);
        waiting->reply = reply;
        waiting->done = true;
    }
    waiting->arrived.notify_all();
}

void LineHost::failAll(const QString &message) {
    QHash<qint64, std::shared_ptr<Waiting>> all;
    {
        std::lock_guard lock(m_mutex);
        all.swap(m_pending);
    }
    const QJsonObject reply{{"error", QJsonObject{{"message", message}, {"status", 503}}}};
    for (const auto &waiting : all) {
        {
            std::lock_guard lock(waiting->mutex);
            waiting->reply = reply;
            waiting->done = true;
        }
        waiting->arrived.notify_all();
    }
}

bool LineHost::busy() const {
    std::lock_guard lock(m_mutex);
    return !m_pending.isEmpty();
}

} // namespace sc::ext
