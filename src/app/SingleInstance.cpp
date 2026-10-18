#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QStandardPaths>

namespace sc {

SingleInstance::SingleInstance(QObject *parent) : QObject(parent) {
    const auto profile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_name = QStringLiteral("scancore-") +
             QString::fromLatin1(QCryptographicHash::hash(profile.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

bool SingleInstance::forwardToRunning(const QStringList &arguments) {
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(400))
        return false;
    socket.write(QJsonDocument(QJsonArray::fromStringList(arguments)).toJson(QJsonDocument::Compact) + '\n');
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

void SingleInstance::listen() {
    // A socket left behind by a crash would otherwise block listening.
    QLocalServer::removeServer(m_name);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(m_name))
        return;
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (auto *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                if (!socket->canReadLine())
                    return;
                const auto line = socket->readLine(64 * 1024);
                QStringList arguments;
                for (const auto &value : QJsonDocument::fromJson(line).array())
                    arguments.append(value.toString());
                emit activated(arguments);
            });
        }
    });
}

} // namespace sc
