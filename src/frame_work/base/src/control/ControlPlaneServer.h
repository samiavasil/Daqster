#pragma once

#include "framework_core_export.h"

#include <capabilities/IRuntimeHost.h> // ControlPlaneOptions

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
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
 * both Qt lines. The WebSocket endpoint (`/api/events`) is hand-rolled on the
 * same socket (RFC 6455) for the same reason — Qt WebSockets would have forced
 * a second listening port and an extra module dependency.
 *
 * Surface:
 *   - REST:     /api/health, /api/nodes, /api/nodes/{id}/{config,schema,start,stop},
 *               /api/flow, /api/openapi.json
 *   - docs:     /api/docs (Swagger UI — from --rest-web-root if vendored, else CDN)
 *   - web UI:   / and /assets/* (bundled console; overridden by --rest-web-root)
 *   - events:   /api/events (WebSocket, RFC 6455)
 *
 * Security model (fail-closed):
 *   - no token + non-loopback bind address => start() refuses (returns false);
 *   - with a token, every request must carry `Authorization: Bearer <token>`;
 *     the comparison is constant-time. WebSocket clients may pass the token as
 *     `?token=` (browsers cannot set headers on WebSocket).
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

    /// Send one event to every connected WebSocket client. Safe when there are
    /// none. @p type is the event name (nodeStarted, nodeStopped,
    /// nodeConfigChanged, nodeError, ...); the wire message is
    /// `{ "type": <type>, "data": <data> }`.
    void broadcastEvent(const QString& type, const QJsonObject& data = QJsonObject());

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket* socket);
    void dispatch(QTcpSocket* socket,
                  const QByteArray& method,
                  const QByteArray& target,
                  const QByteArray& body,
                  const QHash<QByteArray, QByteArray>& headers);

    // API handlers. Each returns true when it produced a response.
    bool handleApi(QTcpSocket* socket,
                   const QByteArray& method,
                   const QString& path,
                   const QByteArray& body);
    void handleFlowReload(QTcpSocket* socket, const QByteArray& body);

    // Static content: bundled web UI + Swagger assets from --rest-web-root.
    bool handleStatic(QTcpSocket* socket, const QString& path);

    void sendJson(QTcpSocket* socket, int status, const QJsonObject& payload);
    void sendBody(QTcpSocket* socket, int status, const QByteArray& contentType,
                  const QByteArray& body);
    void sendHtml(QTcpSocket* socket, int status, const QByteArray& html);

    bool authorize(const QHash<QByteArray, QByteArray>& headers) const;

    // ── WebSocket (RFC 6455, server -> client broadcast only) ──────────────
    bool upgradeToWebSocket(QTcpSocket* socket,
                            const QByteArray& target,
                            const QHash<QByteArray, QByteArray>& headers);
    void handleWebSocketData(QTcpSocket* socket);
    void sendWebSocketFrame(QTcpSocket* socket, const QByteArray& payload, quint8 opcode);
    void dropWebSocketClient(QTcpSocket* socket);

    // Helpers
    bool readWebFile(const QString& path, QByteArray& body, QByteArray& contentType) const;
    QJsonObject buildOpenApiSpec() const;
    QByteArray openApiDocsHtml() const;
    QByteArray bundledIndexHtml() const;

    RuntimeController* m_controller = nullptr;
    QTcpServer m_server;
    QString m_token;
    QString m_webRoot;
    QHash<QTcpSocket*, QByteArray> m_buffers;   ///< pre-dispatch request buffers
    QSet<QTcpSocket*> m_wsClients;              ///< upgraded WebSocket clients
    QHash<QTcpSocket*, QByteArray> m_wsBuffers; ///< partial WebSocket frames
};

} // namespace Daqster
