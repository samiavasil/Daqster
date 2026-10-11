#include "ControlPlaneServer.h"

#include "RuntimeController.h"

#include <LogCategories.h>

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace Daqster {

namespace {

const char* reasonPhrase(int status)
{
    switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 415: return "Unsupported Media Type";
    case 501: return "Not Implemented";
    default:  return "Internal Server Error";
    }
}

/// Constant-time comparison so a wrong token cannot be recovered from timing.
bool constantTimeEquals(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return diff == 0;
}

bool isLoopbackAddress(const QString& address)
{
    const QString a = address.trimmed().toLower();
    if (a.isEmpty())
        return true;
    if (a == QLatin1String("localhost") || a == QLatin1String("127.0.0.1")
        || a == QLatin1String("::1"))
        return true;
    // Any 127.0.0.0/8 address is loopback too.
    return a.startsWith(QLatin1String("127."));
}

QHostAddress toHostAddress(const QString& address)
{
    const QString a = address.trimmed().toLower();
    if (a.isEmpty() || a == QLatin1String("localhost") || a == QLatin1String("127.0.0.1"))
        return QHostAddress(QHostAddress::LocalHost);
    if (a == QLatin1String("::1"))
        return QHostAddress(QHostAddress::LocalHostIPv6);
    return QHostAddress(address);
}

quint64 toNodeId(const QString& text, bool* ok)
{
    return text.toULongLong(ok);
}

} // namespace

ControlPlaneServer::ControlPlaneServer(RuntimeController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
{
    connect(&m_server, &QTcpServer::newConnection, this, &ControlPlaneServer::onNewConnection);
}

ControlPlaneServer::~ControlPlaneServer()
{
    stop();
}

bool ControlPlaneServer::start(const ControlPlaneOptions& options, QString* errorMessage)
{
    if (!m_controller) {
        if (errorMessage) *errorMessage = QStringLiteral("no runtime controller");
        return false;
    }
    if (m_server.isListening())
        return true;

    if (options.authToken.isEmpty() && !isLoopbackAddress(options.bindAddress)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "refusing to bind %1 without an auth token (fail-closed)")
                .arg(options.bindAddress);
        }
        return false;
    }

    m_token = options.authToken;

    const QHostAddress host = toHostAddress(options.bindAddress);
    if (!m_server.listen(host, options.port)) {
        if (errorMessage) *errorMessage = m_server.errorString();
        return false;
    }

    qCInfo(lcApp) << "ControlPlaneServer listening on" << m_server.serverAddress().toString()
                  << ":" << m_server.serverPort()
                  << (m_token.isEmpty() ? "(no auth, loopback only)" : "(bearer token required)");
    return true;
}

void ControlPlaneServer::stop()
{
    for (QTcpSocket* socket : m_buffers.keys())
        socket->deleteLater();
    m_buffers.clear();
    if (m_server.isListening())
        m_server.close();
}

bool ControlPlaneServer::isListening() const { return m_server.isListening(); }
quint16 ControlPlaneServer::port() const { return m_server.serverPort(); }

void ControlPlaneServer::onNewConnection()
{
    while (QTcpSocket* socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void ControlPlaneServer::onReadyRead(QTcpSocket* socket)
{
    auto it = m_buffers.find(socket);
    if (it == m_buffers.end())
        return;

    it->append(socket->readAll());

    // Wait for the full header block.
    const int headerEnd = it->indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    const QByteArray headerBlock = it->left(headerEnd);
    const QList<QByteArray> lines = headerBlock.split('\n');

    QHash<QByteArray, QByteArray> headers;
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }

    int contentLength = 0;
    if (headers.contains("content-length"))
        contentLength = headers.value("content-length").toInt();

    const int bodyStart = headerEnd + 4;
    if (it->size() < bodyStart + contentLength)
        return; // wait for the body

    const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
    const QByteArray method = requestLine.value(0);
    const QByteArray target = requestLine.value(1);
    const QByteArray body = it->mid(bodyStart, contentLength);

    // One request per connection is enough for a control API; close afterwards.
    m_buffers.erase(it);
    socket->disconnect(this);

    dispatch(socket, method, target, body, headers);
}

void ControlPlaneServer::dispatch(QTcpSocket* socket,
                                  const QByteArray& method,
                                  const QByteArray& target,
                                  const QByteArray& body,
                                  const QHash<QByteArray, QByteArray>& headers)
{
    const QUrl url(QString::fromUtf8(target));
    const QString path = url.path();

    if (!authorize(headers)) {
        sendBody(socket, 401, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"unauthorized\"}"));
        return;
    }

    auto requireGet = [&](const char* m) -> bool {
        if (method == m) return true;
        sendBody(socket, 405, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"method not allowed\"}"));
        return false;
    };

    if (path == QLatin1String("/api/health")) {
        if (!requireGet("GET")) return;
        QJsonObject out;
        out.insert(QStringLiteral("status"), QStringLiteral("ok"));
        out.insert(QStringLiteral("nodes"), m_controller->nodes().size());
        sendJson(socket, 200, out);
        return;
    }

    if (path == QLatin1String("/api/openapi.json")) {
        if (!requireGet("GET")) return;
        QJsonObject paths;
        for (const char* p : {"/api/health", "/api/nodes", "/api/flow"}) {
            QJsonObject op; op.insert(QStringLiteral("get"), QJsonObject());
            paths.insert(QString::fromLatin1(p), op);
        }
        QJsonObject info;
        info.insert(QStringLiteral("title"), QStringLiteral("Daqster control plane"));
        info.insert(QStringLiteral("version"), QStringLiteral("0.1"));
        QJsonObject spec;
        spec.insert(QStringLiteral("openapi"), QStringLiteral("3.0.0"));
        spec.insert(QStringLiteral("info"), info);
        spec.insert(QStringLiteral("paths"), paths);
        sendJson(socket, 200, spec);
        return;
    }

    if (path == QLatin1String("/api/nodes")) {
        if (!requireGet("GET")) return;
        QJsonArray arr;
        for (const RuntimeController::NodeInfo& n : m_controller->nodes()) {
            QJsonObject o;
            o.insert(QStringLiteral("id"), static_cast<qint64>(n.id));
            o.insert(QStringLiteral("type"), n.type);
            o.insert(QStringLiteral("name"), n.name);
            o.insert(QStringLiteral("state"), n.state);
            arr.append(o);
        }
        QJsonObject out;
        out.insert(QStringLiteral("nodes"), arr);
        sendJson(socket, 200, out);
        return;
    }

    if (path == QLatin1String("/api/flow")) {
        if (!requireGet("GET")) return;
        sendJson(socket, 200, m_controller->flowJson());
        return;
    }

    // /api/nodes/{id}/<action>
    if (path.startsWith(QLatin1String("/api/nodes/"))) {
        const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        // ["api","nodes","<id>","<action>"]
        if (parts.size() == 3 && parts.at(1) == QLatin1String("nodes")) {
            bool ok = false;
            const quint64 id = toNodeId(parts.at(2), &ok);
            if (!ok) { sendJson(socket, 400, {{QStringLiteral("error"), QStringLiteral("bad node id")}}); return; }
            QJsonObject cfg;
            if (method == "GET" && m_controller->nodeConfig(id, cfg)) { sendJson(socket, 200, cfg); return; }
            sendBody(socket, 404, QByteArrayLiteral("application/json"),
                     QByteArrayLiteral("{\"error\":\"node not found\"}"));
            return;
        }
        if (parts.size() == 4 && parts.at(1) == QLatin1String("nodes")) {
            bool ok = false;
            const quint64 id = toNodeId(parts.at(2), &ok);
            const QString action = parts.at(3);
            if (!ok) { sendJson(socket, 400, {{QStringLiteral("error"), QStringLiteral("bad node id")}}); return; }

            if (action == QLatin1String("config")) {
                if (method == "GET") {
                    QJsonObject cfg;
                    if (m_controller->nodeConfig(id, cfg)) { sendJson(socket, 200, cfg); return; }
                    sendBody(socket, 404, QByteArrayLiteral("application/json"),
                             QByteArrayLiteral("{\"error\":\"node not found\"}"));
                    return;
                }
                if (method == "POST") {
                    QJsonParseError err{};
                    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
                    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                        sendBody(socket, 400, QByteArrayLiteral("application/json"),
                                 QByteArrayLiteral("{\"error\":\"invalid JSON body\"}"));
                        return;
                    }
                    const bool applied = m_controller->setNodeConfig(id, doc.object());
                    sendJson(socket, applied ? 200 : 400,
                             {{QStringLiteral("applied"), applied}});
                    return;
                }
                sendBody(socket, 405, QByteArrayLiteral("application/json"),
                         QByteArrayLiteral("{\"error\":\"method not allowed\"}"));
                return;
            }
            if (action == QLatin1String("start") || action == QLatin1String("stop")) {
                if (method != "POST") {
                    sendBody(socket, 405, QByteArrayLiteral("application/json"),
                             QByteArrayLiteral("{\"error\":\"method not allowed\"}"));
                    return;
                }
                const bool okAction = (action == QLatin1String("start"))
                    ? m_controller->startNode(id) : m_controller->stopNode(id);
                sendJson(socket, okAction ? 200 : 400, {{QStringLiteral("ok"), okAction}});
                return;
            }
            if (action == QLatin1String("schema")) {
                if (method != "GET") {
                    sendBody(socket, 405, QByteArrayLiteral("application/json"),
                             QByteArrayLiteral("{\"error\":\"method not allowed\"}"));
                    return;
                }
                sendJson(socket, 200, m_controller->nodeSchema(id));
                return;
            }
        }
    }

    sendBody(socket, 404, QByteArrayLiteral("application/json"),
             QByteArrayLiteral("{\"error\":\"not found\"}"));
}

bool ControlPlaneServer::authorize(const QHash<QByteArray, QByteArray>& headers) const
{
    if (m_token.isEmpty())
        return true; // loopback-only: enforced at start()

    const QByteArray auth = headers.value("authorization");
    const QByteArray expected = QByteArrayLiteral("Bearer ") + m_token.toUtf8();
    return constantTimeEquals(auth, expected);
}

void ControlPlaneServer::sendJson(QTcpSocket* socket, int status, const QJsonObject& payload)
{
    sendBody(socket, status, QByteArrayLiteral("application/json"),
             QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void ControlPlaneServer::sendBody(QTcpSocket* socket, int status, const QByteArray& contentType,
                                  const QByteArray& body)
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(status) + ' ' + reasonPhrase(status) + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
}

} // namespace Daqster
