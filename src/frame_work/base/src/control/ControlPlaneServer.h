#pragma once

#include "framework_core_export.h"

#include <capabilities/IRuntimeHost.h> // ControlPlaneOptions

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpServer>

class QTcpSocket;

namespace Daqster {

class RuntimeController;

/**
 * @brief Minimal, dependency-free HTTP control plane for a running flow.
 *
 * REQ-SW-PL-054. The server speaks HTTP/1.1 directly on QTcpServer instead of
 * QHttpServer, because the runner must build on both Qt5 (no QtHttpServer) and
 * Qt6. One implementation keeps the API and auth behaviour identical across
 * both Qt lines.
 *
 * Security model (fail-closed):
 *   - no token + non-loopback bind address => start() refuses (returns false);
 *   - with a token, every request must carry `Authorization: Bearer <token>`;
 *     the comparison is constant-time.
 *
 * It is deliberately read-mostly for this first slice: list/config/flow/start/
 * stop/health/openapi. WebSocket events and the bundled web UI are follow-ups.
 */
class FRAMEWORK_CORE_EXPORT ControlPlaneServer : public QObject
{
    Q_OBJECT
public:
    explicit ControlPlaneServer(RuntimeController* controller, QObject* parent = nullptr);
    ~ControlPlaneServer() override;

    /// Bind + start listening. Returns false and fills @p errorMessage when the
    /// bind address is refused (e.g. non-loopback without a token) or the port
    /// is unavailable.
    bool start(const ControlPlaneOptions& options, QString* errorMessage = nullptr);
    void stop();

    bool isListening() const;
    quint16 port() const;

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket* socket);
    void dispatch(QTcpSocket* socket,
                  const QByteArray& method,
                  const QByteArray& target,
                  const QByteArray& body,
                  const QHash<QByteArray, QByteArray>& headers);

    void sendJson(QTcpSocket* socket, int status, const QJsonObject& payload);
    void sendBody(QTcpSocket* socket, int status, const QByteArray& contentType,
                  const QByteArray& body);

    bool authorize(const QHash<QByteArray, QByteArray>& headers) const;

    RuntimeController* m_controller = nullptr;
    QTcpServer m_server;
    QString m_token;
    QHash<QTcpSocket*, QByteArray> m_buffers;
};

} // namespace Daqster
