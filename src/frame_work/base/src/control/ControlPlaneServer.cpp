#include "ControlPlaneServer.h"

#include "RuntimeController.h"

#include <LogCategories.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 415: return "Unsupported Media Type";
    case 500: return "Internal Server Error";
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

/// Content-Type for the handful of asset kinds the web UI and Swagger UI ship.
QByteArray mimeTypeForPath(const QString& path)
{
    const QString p = path.toLower();
    if (p.endsWith(QLatin1String(".html")) || p.endsWith(QLatin1String(".htm")))
        return QByteArrayLiteral("text/html; charset=utf-8");
    if (p.endsWith(QLatin1String(".js")) || p.endsWith(QLatin1String(".mjs")))
        return QByteArrayLiteral("text/javascript; charset=utf-8");
    if (p.endsWith(QLatin1String(".css")))
        return QByteArrayLiteral("text/css; charset=utf-8");
    if (p.endsWith(QLatin1String(".json")))
        return QByteArrayLiteral("application/json");
    if (p.endsWith(QLatin1String(".map")))
        return QByteArrayLiteral("application/json");
    if (p.endsWith(QLatin1String(".svg")))
        return QByteArrayLiteral("image/svg+xml");
    if (p.endsWith(QLatin1String(".png")))
        return QByteArrayLiteral("image/png");
    if (p.endsWith(QLatin1String(".ico")))
        return QByteArrayLiteral("image/x-icon");
    if (p.endsWith(QLatin1String(".woff2")))
        return QByteArrayLiteral("font/woff2");
    if (p.endsWith(QLatin1String(".woff")))
        return QByteArrayLiteral("font/woff");
    return QByteArrayLiteral("application/octet-stream");
}

/// RFC 6455 handshake accept value.
QByteArray webSocketAccept(const QByteArray& clientKey)
{
    static const QByteArray guid = QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    return QCryptographicHash::hash(clientKey + guid, QCryptographicHash::Sha1).toBase64();
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
    m_webRoot = options.webRoot;

    const QHostAddress host = toHostAddress(options.bindAddress);
    if (!m_server.listen(host, options.port)) {
        if (errorMessage) *errorMessage = m_server.errorString();
        return false;
    }

    qCInfo(lcApp) << "ControlPlaneServer listening on" << m_server.serverAddress().toString()
                  << ":" << m_server.serverPort()
                  << (m_token.isEmpty() ? "(no auth, loopback only)" : "(bearer token required)")
                  << (m_webRoot.isEmpty() ? "" : QStringLiteral("web-root=%1").arg(m_webRoot));
    return true;
}

void ControlPlaneServer::stop()
{
    const QSet<QTcpSocket*> ws = m_wsClients;
    for (QTcpSocket* socket : ws) {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    m_wsClients.clear();
    m_wsBuffers.clear();

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
            m_wsClients.remove(socket);
            m_wsBuffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void ControlPlaneServer::onReadyRead(QTcpSocket* socket)
{
    if (m_wsClients.contains(socket)) {
        handleWebSocketData(socket);
        return;
    }

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

    // The connection is now owned by dispatch(): a normal response closes it,
    // a WebSocket upgrade keeps it and moves it to m_wsClients.
    m_buffers.erase(it);

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

    // CORS preflight must succeed without authentication (REQ-SW-PL-054 AC12).
    if (method == "OPTIONS") {
        sendBody(socket, 204, QByteArrayLiteral("text/plain"), QByteArray());
        return;
    }

    // WebSocket upgrade (REQ-SW-PL-054 AC10). Browsers cannot set an
    // Authorization header on a WebSocket, so this path accepts ?token= too.
    const bool wantsUpgrade = headers.value("upgrade").toLower().contains("websocket");
    if (wantsUpgrade && path == QLatin1String("/api/events")) {
        upgradeToWebSocket(socket, target, headers);
        return;
    }

    const bool isApi = path.startsWith(QLatin1String("/api/"));
    // The docs page and the spec it loads are UI, not data: they must be
    // reachable by a plain browser navigation (which carries no token).
    const bool publicApi = path == QLatin1String("/api/docs")
                        || path == QLatin1String("/api/docs/")
                        || path == QLatin1String("/api/openapi.json");

    if (isApi && !publicApi && !authorize(headers)) {
        sendBody(socket, 401, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"unauthorized\"}"));
        return;
    }

    if (isApi) {
        if (handleApi(socket, method, path, body))
            return;
        sendBody(socket, 404, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"not found\"}"));
        return;
    }

    if (method == "GET" && handleStatic(socket, path))
        return;

    sendBody(socket, 404, QByteArrayLiteral("application/json"),
             QByteArrayLiteral("{\"error\":\"not found\"}"));
}

bool ControlPlaneServer::handleApi(QTcpSocket* socket,
                                   const QByteArray& method,
                                   const QString& path,
                                   const QByteArray& body)
{
    auto methodNotAllowed = [&]() {
        sendBody(socket, 405, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"method not allowed\"}"));
    };

    if (path == QLatin1String("/api/health")) {
        if (method != "GET") { methodNotAllowed(); return true; }
        QJsonObject out;
        out.insert(QStringLiteral("status"), QStringLiteral("ok"));
        out.insert(QStringLiteral("nodes"), m_controller->nodes().size());
        sendJson(socket, 200, out);
        return true;
    }

    if (path == QLatin1String("/api/openapi.json")) {
        if (method != "GET") { methodNotAllowed(); return true; }
        sendJson(socket, 200, buildOpenApiSpec());
        return true;
    }

    if (path == QLatin1String("/api/docs") || path == QLatin1String("/api/docs/")) {
        if (method != "GET") { methodNotAllowed(); return true; }
        sendHtml(socket, 200, openApiDocsHtml());
        return true;
    }

    if (path == QLatin1String("/api/nodes")) {
        if (method != "GET") { methodNotAllowed(); return true; }
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
        return true;
    }

    if (path == QLatin1String("/api/flow")) {
        if (method == "GET") {
            sendJson(socket, 200, m_controller->flowJson());
            return true;
        }
        if (method == "POST") {
            handleFlowReload(socket, body);
            return true;
        }
        methodNotAllowed();
        return true;
    }

    // /api/nodes/{id}[/<action>]
    if (path.startsWith(QLatin1String("/api/nodes/"))) {
        const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        // ["api","nodes","<id>","<action>"]
        if (parts.size() == 3 && parts.at(1) == QLatin1String("nodes")) {
            bool ok = false;
            const quint64 id = toNodeId(parts.at(2), &ok);
            if (!ok) { sendJson(socket, 400, {{QStringLiteral("error"), QStringLiteral("bad node id")}}); return true; }
            if (method == "GET") {
                QJsonObject cfg;
                if (m_controller->nodeConfig(id, cfg)) { sendJson(socket, 200, cfg); return true; }
                sendBody(socket, 404, QByteArrayLiteral("application/json"),
                         QByteArrayLiteral("{\"error\":\"node not found\"}"));
                return true;
            }
            methodNotAllowed();
            return true;
        }
        if (parts.size() == 4 && parts.at(1) == QLatin1String("nodes")) {
            bool ok = false;
            const quint64 id = toNodeId(parts.at(2), &ok);
            const QString action = parts.at(3);
            if (!ok) { sendJson(socket, 400, {{QStringLiteral("error"), QStringLiteral("bad node id")}}); return true; }

            if (action == QLatin1String("config")) {
                if (method == "GET") {
                    QJsonObject cfg;
                    if (m_controller->nodeConfig(id, cfg)) { sendJson(socket, 200, cfg); return true; }
                    sendBody(socket, 404, QByteArrayLiteral("application/json"),
                             QByteArrayLiteral("{\"error\":\"node not found\"}"));
                    return true;
                }
                if (method == "POST") {
                    QJsonParseError err{};
                    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
                    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                        sendBody(socket, 400, QByteArrayLiteral("application/json"),
                                 QByteArrayLiteral("{\"error\":\"invalid JSON body\"}"));
                        return true;
                    }
                    const bool applied = m_controller->setNodeConfig(id, doc.object());
                    sendJson(socket, applied ? 200 : 400,
                             {{QStringLiteral("applied"), applied}});
                    return true;
                }
                methodNotAllowed();
                return true;
            }
            if (action == QLatin1String("start") || action == QLatin1String("stop")) {
                if (method != "POST") { methodNotAllowed(); return true; }
                const bool okAction = (action == QLatin1String("start"))
                    ? m_controller->startNode(id) : m_controller->stopNode(id);
                sendJson(socket, okAction ? 200 : 400, {{QStringLiteral("ok"), okAction}});
                return true;
            }
            if (action == QLatin1String("schema")) {
                if (method != "GET") { methodNotAllowed(); return true; }
                sendJson(socket, 200, m_controller->nodeSchema(id));
                return true;
            }
        }
    }

    return false;
}

void ControlPlaneServer::handleFlowReload(QTcpSocket* socket, const QByteArray& body)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        sendBody(socket, 400, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"invalid JSON body\"}"));
        return;
    }

    QJsonObject flow = doc.object();

    // Convenience: `{ "path": "/abs/file.flow" }` hot-reloads from disk, so the
    // documented curl flow matches REQ-SW-PL-054 verification.
    if (flow.contains(QStringLiteral("path")) && !flow.contains(QStringLiteral("nodes"))) {
        const QString flowPath = flow.value(QStringLiteral("path")).toString();
        QFile file(flowPath);
        if (flowPath.isEmpty() || !file.open(QIODevice::ReadOnly)) {
            sendJson(socket, 400, {{QStringLiteral("error"),
                                    QStringLiteral("cannot open flow file: %1").arg(flowPath)}});
            return;
        }
        QJsonParseError fileErr{};
        const QJsonDocument fileDoc = QJsonDocument::fromJson(file.readAll(), &fileErr);
        if (fileErr.error != QJsonParseError::NoError || !fileDoc.isObject()) {
            sendJson(socket, 400, {{QStringLiteral("error"),
                                    QStringLiteral("invalid JSON in flow file: %1").arg(fileErr.errorString())}});
            return;
        }
        flow = fileDoc.object();
    }

    // Reject structurally wrong documents instead of silently loading an empty
    // scene (a `"nodes": "nope"` would otherwise pass as zero nodes).
    if ((flow.contains(QStringLiteral("nodes")) && !flow.value(QStringLiteral("nodes")).isArray())
        || (flow.contains(QStringLiteral("connections"))
            && !flow.value(QStringLiteral("connections")).isArray())
        || (flow.contains(QStringLiteral("ui")) && !flow.value(QStringLiteral("ui")).isObject())) {
        sendJson(socket, 400, {{QStringLiteral("error"),
                                QStringLiteral("flow fields must be: nodes[], connections[], ui{}")}});
        return;
    }

    QString error;
    const bool applied = m_controller->reloadFlow(flow, &error);
    QJsonObject out;
    out.insert(QStringLiteral("applied"), applied);
    if (!applied)
        out.insert(QStringLiteral("error"), error);
    sendJson(socket, applied ? 200 : 400, out);
}

// ═══════════════════════════════════════════════════════════════════════════
// Static content (bundled web UI + Swagger UI assets)
// ═══════════════════════════════════════════════════════════════════════════

bool ControlPlaneServer::handleStatic(QTcpSocket* socket, const QString& path)
{
    if (path == QLatin1String("/") || path == QLatin1String("/index.html")) {
        QByteArray body;
        QByteArray contentType;
        if (readWebFile(QStringLiteral("/index.html"), body, contentType)) {
            sendBody(socket, 200, contentType, body);
            return true;
        }
        sendHtml(socket, 200, bundledIndexHtml());
        return true;
    }

    // Everything else is a best-effort file from --rest-web-root (assets/*,
    // favicon, ...). Missing files fall through to the caller's 404.
    QByteArray body;
    QByteArray contentType;
    if (readWebFile(path, body, contentType)) {
        sendBody(socket, 200, contentType, body);
        return true;
    }
    return false;
}

bool ControlPlaneServer::readWebFile(const QString& path, QByteArray& body,
                                     QByteArray& contentType) const
{
    if (m_webRoot.isEmpty())
        return false;

    QString rel = path;
    while (rel.startsWith(QLatin1Char('/')))
        rel.remove(0, 1);
    if (rel.isEmpty() || rel.contains(QLatin1String("..")))
        return false;

    const QDir root(m_webRoot);
    const QString abs = root.absoluteFilePath(rel);
    // Never serve outside the configured root, even through symlinks.
    const QString canonicalRoot = QFileInfo(m_webRoot).canonicalFilePath();
    const QString canonicalFile = QFileInfo(abs).canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalFile.isEmpty()
        || !canonicalFile.startsWith(canonicalRoot))
        return false;

    QFile file(canonicalFile);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    body = file.readAll();
    contentType = mimeTypeForPath(canonicalFile);
    return true;
}

QByteArray ControlPlaneServer::openApiDocsHtml() const
{
    const bool localAssets = !m_webRoot.isEmpty()
        && QFile::exists(m_webRoot + QLatin1String("/assets/swagger-ui/swagger-ui-bundle.js"));

    const QString css = localAssets
        ? QStringLiteral("/assets/swagger-ui/swagger-ui.css")
        : QStringLiteral("https://unpkg.com/swagger-ui-dist@5.33.1/swagger-ui.css");
    const QString bundle = localAssets
        ? QStringLiteral("/assets/swagger-ui/swagger-ui-bundle.js")
        : QStringLiteral("https://unpkg.com/swagger-ui-dist@5.33.1/swagger-ui-bundle.js");
    const QString standalone = localAssets
        ? QStringLiteral("/assets/swagger-ui/swagger-ui-standalone-preset.js")
        : QStringLiteral("https://unpkg.com/swagger-ui-dist@5.33.1/swagger-ui-standalone-preset.js");

    static const char* kTemplate = R"DAQ(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Daqster Control Plane — API</title>
  <link rel="stylesheet" href="%CSS%">
  <style>body{margin:0;background:#fafafa}</style>
</head>
<body>
  <div id="swagger-ui"></div>
  <script src="%BUNDLE%"></script>
  <script src="%STANDALONE%"></script>
  <script>
    window.onload = function () {
      SwaggerUIBundle({
        url: "/api/openapi.json",
        dom_id: "#swagger-ui",
        deepLinking: true,
        presets: [SwaggerUIBundle.presets.apis, SwaggerUIStandalonePreset],
        layout: "StandaloneLayout"
      });
    };
  </script>
</body>
</html>
)DAQ";

    QString html = QString::fromUtf8(kTemplate);
    html.replace(QStringLiteral("%CSS%"), css);
    html.replace(QStringLiteral("%BUNDLE%"), bundle);
    html.replace(QStringLiteral("%STANDALONE%"), standalone);
    return html.toUtf8();
}

QByteArray ControlPlaneServer::bundledIndexHtml() const
{
    static const char* kHtml = R"DAQ(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Daqster Control Plane</title>
<style>
 body{font-family:system-ui,Segoe UI,sans-serif;margin:0;background:#0f1115;color:#e7e9ee}
 header{padding:10px 16px;background:#171a21;border-bottom:1px solid #262b36;display:flex;gap:10px;align-items:center}
 h1{font-size:14px;margin:0;font-weight:600;letter-spacing:.02em}
 header a{color:#7fb0ff;font-size:12px;margin-left:auto;text-decoration:none}
 main{padding:16px;display:grid;grid-template-columns:1.1fr .9fr;gap:16px}
 section{background:#171a21;border:1px solid #262b36;border-radius:8px;padding:12px}
 h2{font-size:11px;margin:0 0 8px;text-transform:uppercase;letter-spacing:.08em;color:#8b93a7}
 table{width:100%;border-collapse:collapse;font-size:13px}
 th,td{text-align:left;padding:4px 6px;border-bottom:1px solid #232833;vertical-align:middle}
 th{color:#8b93a7;font-weight:500}
 button{background:#2b6cff;border:0;color:#fff;border-radius:5px;padding:3px 8px;cursor:pointer;font-size:12px}
 button.sec{background:#3a4152}
 input{background:#0d0f13;border:1px solid #2a2f3a;color:#e7e9ee;border-radius:5px;padding:5px 7px;font:12px monospace;width:180px}
 pre{white-space:pre-wrap;word-break:break-word;font:12px/1.45 monospace;margin:0;max-height:300px;overflow:auto}
 .pill{padding:1px 7px;border-radius:999px;font-size:11px}
 .running{background:#123d21;color:#5ff08a}.stopped{background:#3a1a1a;color:#ff8b8b}
 .muted{color:#8b93a7;font-size:12px}
</style>
</head>
<body>
<header>
  <h1>Daqster Control Plane</h1>
  <input id="token" placeholder="Bearer token (optional)">
  <button onclick="reconnect()">Connect</button>
  <a href="/api/docs">API docs (Swagger UI)</a>
</header>
<main>
  <section>
    <h2>Nodes</h2>
    <table><thead><tr><th>id</th><th>name</th><th>type</th><th>state</th><th></th></tr></thead>
    <tbody id="nodes"><tr><td colspan="5" class="muted">not connected</td></tr></tbody></table>
  </section>
  <section>
    <h2>Events (WebSocket /api/events)</h2>
    <pre id="events">press Connect to subscribe</pre>
  </section>
</main>
<script>
const state = { token: localStorage.getItem("daqster.token") || "", ws: null };
document.getElementById("token").value = state.token;

function api(path, opts) {
  opts = opts || {};
  opts.headers = Object.assign({ "Content-Type": "application/json" }, opts.headers || {});
  if (state.token) opts.headers["Authorization"] = "Bearer " + state.token;
  return fetch(path, opts).then(r => r.json().then(b => ({ ok: r.ok, status: r.status, body: b })));
}

function logLine(text) {
  const el = document.getElementById("events");
  const line = document.createElement("div");
  line.textContent = new Date().toLocaleTimeString() + "  " + text;
  el.appendChild(line);
  el.scrollTop = el.scrollHeight;
}

function render(nodes) {
  const body = document.getElementById("nodes");
  body.innerHTML = "";
  if (!nodes.length) { body.innerHTML = '<tr><td colspan="5" class="muted">no nodes</td></tr>'; return; }
  nodes.forEach(n => {
    const tr = document.createElement("tr");
    tr.innerHTML = "<td>" + n.id + "</td><td>" + n.name + "</td><td>" + n.type +
      '</td><td><span class="pill ' + n.state + '">' + n.state + "</span></td><td></td>";
    const cell = tr.lastElementChild;
    const start = document.createElement("button");
    start.textContent = "start";
    start.onclick = () => api("/api/nodes/" + n.id + "/start", { method: "POST" }).then(refresh);
    const stop = document.createElement("button");
    stop.textContent = "stop";
    stop.className = "sec";
    stop.onclick = () => api("/api/nodes/" + n.id + "/stop", { method: "POST" }).then(refresh);
    cell.appendChild(start); cell.appendChild(stop);
    body.appendChild(tr);
  });
}

function refresh() { return api("/api/nodes").then(r => render(r.body.nodes || [])); }

function connectSocket() {
  if (state.ws) { state.ws.onclose = null; state.ws.close(); state.ws = null; }
  const proto = location.protocol === "https:" ? "wss" : "ws";
  const q = state.token ? "?token=" + encodeURIComponent(state.token) : "";
  const ws = new WebSocket(proto + "://" + location.host + "/api/events" + q);
  state.ws = ws;
  ws.onopen = () => logLine("connected");
  ws.onmessage = e => { try { const m = JSON.parse(e.data); logLine(m.type + " " + JSON.stringify(m.data)); } catch (_) { logLine(e.data); } };
  ws.onclose = () => logLine("disconnected");
  ws.onerror = () => logLine("socket error");
}

function reconnect() {
  state.token = document.getElementById("token").value.trim();
  localStorage.setItem("daqster.token", state.token);
  refresh().then(connectSocket);
}

refresh().then(connectSocket);
</script>
</body>
</html>
)DAQ";
    return QByteArray(kHtml);
}

// ═══════════════════════════════════════════════════════════════════════════
// OpenAPI 3.0 description (REQ-SW-PL-054 AC13)
// ═══════════════════════════════════════════════════════════════════════════

namespace {

QJsonObject jsonResponse(const QString& description)
{
    QJsonObject r;
    r.insert(QStringLiteral("description"), description);
    return r;
}

QJsonObject refSchema(const QString& name)
{
    QJsonObject s;
    s.insert(QStringLiteral("$ref"), QStringLiteral("#/components/schemas/") + name);
    return s;
}

QJsonObject jsonContent(const QString& schemaRef)
{
    QJsonObject schema = refSchema(schemaRef);
    QJsonObject media;
    media.insert(QStringLiteral("schema"), schema);
    QJsonObject content;
    content.insert(QStringLiteral("application/json"), media);
    return content;
}

QJsonObject okResponse(const QString& description, const QString& schemaRef)
{
    QJsonObject r = jsonResponse(description);
    if (!schemaRef.isEmpty())
        r.insert(QStringLiteral("content"), jsonContent(schemaRef));
    return r;
}

QJsonObject pathParam(const QString& name, const QString& description)
{
    QJsonObject p;
    p.insert(QStringLiteral("name"), name);
    p.insert(QStringLiteral("in"), QStringLiteral("path"));
    p.insert(QStringLiteral("required"), true);
    QJsonObject t;
    t.insert(QStringLiteral("type"), QStringLiteral("integer"));
    t.insert(QStringLiteral("format"), QStringLiteral("int64"));
    p.insert(QStringLiteral("schema"), t);
    p.insert(QStringLiteral("description"), description);
    return p;
}

QJsonObject requestJsonBody(const QString& schemaRef)
{
    QJsonObject body;
    body.insert(QStringLiteral("required"), true);
    body.insert(QStringLiteral("content"), jsonContent(schemaRef));
    return body;
}

} // namespace

QJsonObject ControlPlaneServer::buildOpenApiSpec() const
{
    QJsonObject info;
    info.insert(QStringLiteral("title"), QStringLiteral("Daqster Control Plane"));
    info.insert(QStringLiteral("version"), QStringLiteral("0.2.0"));
    info.insert(QStringLiteral("description"),
                QStringLiteral("REST + WebSocket control plane for a running Daqster flow "
                               "(REQ-SW-PL-054). Node config is the generic "
                               "NodeDelegateModel::save()/load() JSON, so every node type "
                               "works without server-side boilerplate."));

    QJsonArray servers;
    {
        QJsonObject s;
        s.insert(QStringLiteral("url"), QStringLiteral("/"));
        s.insert(QStringLiteral("description"), QStringLiteral("This server"));
        servers.append(s);
    }

    // Security schemes: bearer token when one was configured.
    QJsonObject bearer;
    bearer.insert(QStringLiteral("type"), QStringLiteral("http"));
    bearer.insert(QStringLiteral("scheme"), QStringLiteral("bearer"));
    QJsonObject schemes;
    schemes.insert(QStringLiteral("bearerAuth"), bearer);
    QJsonObject components;
    components.insert(QStringLiteral("securitySchemes"), schemes);

    QJsonObject paths;

    {
        QJsonObject get;
        get.insert(QStringLiteral("tags"), QJsonArray{QStringLiteral("health")});
        get.insert(QStringLiteral("summary"), QStringLiteral("Liveness probe"));
        get.insert(QStringLiteral("operationId"), QStringLiteral("getHealth"));
        QJsonObject responses;
        responses.insert(QStringLiteral("200"), okResponse(QStringLiteral("Server is alive"), QStringLiteral("Health")));
        get.insert(QStringLiteral("responses"), responses);
        QJsonObject p;
        p.insert(QStringLiteral("get"), get);
        paths.insert(QStringLiteral("/api/health"), p);
    }

    {
        QJsonObject get;
        get.insert(QStringLiteral("tags"), QJsonArray{QStringLiteral("flow")});
        get.insert(QStringLiteral("summary"), QStringLiteral("List nodes"));
        get.insert(QStringLiteral("operationId"), QStringLiteral("listNodes"));
        QJsonObject responses;
        responses.insert(QStringLiteral("200"), okResponse(QStringLiteral("Current nodes"), QStringLiteral("NodeList")));
        get.insert(QStringLiteral("responses"), responses);
        QJsonObject p;
        p.insert(QStringLiteral("get"), get);
        paths.insert(QStringLiteral("/api/nodes"), p);
    }

    {
        QJsonObject get;
        get.insert(QStringLiteral("tags"), QJsonArray{QStringLiteral("flow")});
        get.insert(QStringLiteral("summary"), QStringLiteral("Get the whole flow (nodes, connections, ui)"));
        get.insert(QStringLiteral("operationId"), QStringLiteral("getFlow"));
        QJsonObject responses;
        responses.insert(QStringLiteral("200"), okResponse(QStringLiteral("Flow document"), QStringLiteral("Flow")));
        get.insert(QStringLiteral("responses"), responses);

        QJsonObject post;
        post.insert(QStringLiteral("tags"), QJsonArray{QStringLiteral("flow")});
        post.insert(QStringLiteral("summary"), QStringLiteral("Hot-reload the flow"));
        post.insert(QStringLiteral("description"),
                    QStringLiteral("Stops every running node, rebuilds the graph from the posted "
                                   "flow document and starts its autoStart nodes. The body is "
                                   "either a full flow object or { \"path\": \"/abs/file.flow\" }."));
        post.insert(QStringLiteral("operationId"), QStringLiteral("reloadFlow"));
        post.insert(QStringLiteral("requestBody"), requestJsonBody(QStringLiteral("Flow")));
        QJsonObject postResponses;
        postResponses.insert(QStringLiteral("200"), okResponse(QStringLiteral("Flow reloaded"), QStringLiteral("MutationResult")));
        postResponses.insert(QStringLiteral("400"), jsonResponse(QStringLiteral("Invalid flow")));
        post.insert(QStringLiteral("responses"), postResponses);

        QJsonObject p;
        p.insert(QStringLiteral("get"), get);
        p.insert(QStringLiteral("post"), post);
        paths.insert(QStringLiteral("/api/flow"), p);
    }

    auto addNodePath = [&](const QString& suffix, const QString& summary, const QString& opId,
                           const QString& method, const QString& schemaRef,
                           const QString& okSchema, bool hasBody) {
        QJsonObject op;
        op.insert(QStringLiteral("tags"), QJsonArray{QStringLiteral("nodes")});
        op.insert(QStringLiteral("summary"), summary);
        op.insert(QStringLiteral("operationId"), opId);
        QJsonArray params;
        params.append(pathParam(QStringLiteral("id"), QStringLiteral("Node id")));
        op.insert(QStringLiteral("parameters"), params);
        if (hasBody)
            op.insert(QStringLiteral("requestBody"), requestJsonBody(schemaRef));
        QJsonObject responses;
        responses.insert(QStringLiteral("200"), okResponse(QStringLiteral("Success"), okSchema));
        responses.insert(QStringLiteral("400"), jsonResponse(QStringLiteral("Rejected")));
        responses.insert(QStringLiteral("404"), jsonResponse(QStringLiteral("Node not found")));
        op.insert(QStringLiteral("responses"), responses);
        QJsonObject p = paths.value(QStringLiteral("/api/nodes/{id}") + suffix).toObject();
        p.insert(method, op);
        paths.insert(QStringLiteral("/api/nodes/{id}") + suffix, p);
    };

    addNodePath(QString(), QStringLiteral("Get a node's current configuration"),
                QStringLiteral("getNodeConfig"), QStringLiteral("get"),
                QString(), QStringLiteral("NodeConfig"), false);
    addNodePath(QStringLiteral("/config"), QStringLiteral("Update a node's configuration"),
                QStringLiteral("setNodeConfig"), QStringLiteral("post"),
                QStringLiteral("NodeConfig"), QStringLiteral("MutationResult"), true);
    addNodePath(QStringLiteral("/schema"), QStringLiteral("JSON Schema for a node's config"),
                QStringLiteral("getNodeSchema"), QStringLiteral("get"),
                QString(), QStringLiteral("JsonSchema"), false);
    addNodePath(QStringLiteral("/start"), QStringLiteral("Start a node"),
                QStringLiteral("startNode"), QStringLiteral("post"),
                QString(), QStringLiteral("MutationResult"), false);
    addNodePath(QStringLiteral("/stop"), QStringLiteral("Stop a node"),
                QStringLiteral("stopNode"), QStringLiteral("post"),
                QString(), QStringLiteral("MutationResult"), false);

    {
        QJsonObject get;
        get.insert(QStringLiteral("tags"), QJsonArray{QStringLiteral("events")});
        get.insert(QStringLiteral("summary"), QStringLiteral("WebSocket event stream"));
        get.insert(QStringLiteral("description"),
                   QStringLiteral("RFC 6455 upgrade. Messages are { type, data }; types: "
                                  "nodeStarted, nodeStopped, nodeConfigChanged, nodeError. "
                                  "Browsers may authenticate with ?token=<bearer>."));
        get.insert(QStringLiteral("operationId"), QStringLiteral("events"));
        QJsonObject responses;
        responses.insert(QStringLiteral("101"), jsonResponse(QStringLiteral("Switching Protocols")));
        responses.insert(QStringLiteral("401"), jsonResponse(QStringLiteral("Unauthorized")));
        get.insert(QStringLiteral("responses"), responses);
        QJsonObject p;
        p.insert(QStringLiteral("get"), get);
        paths.insert(QStringLiteral("/api/events"), p);
    }

    {
        QJsonObject get;
        get.insert(QStringLiteral("tags"), QJsonArray{QStringLiteral("meta")});
        get.insert(QStringLiteral("summary"), QStringLiteral("This document"));
        get.insert(QStringLiteral("operationId"), QStringLiteral("getOpenApi"));
        QJsonObject responses;
        responses.insert(QStringLiteral("200"),
                         okResponse(QStringLiteral("OpenAPI document"), QString()));
        get.insert(QStringLiteral("responses"), responses);
        QJsonObject p;
        p.insert(QStringLiteral("get"), get);
        paths.insert(QStringLiteral("/api/openapi.json"), p);
    }

    // ── Component schemas ─────────────────────────────────────────────────
    QJsonObject schemas;

    schemas.insert(QStringLiteral("Health"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("status"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
            {QStringLiteral("nodes"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}}}}});

    schemas.insert(QStringLiteral("Node"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("id"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("format"), QStringLiteral("int64")}}},
            {QStringLiteral("type"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("description"), QStringLiteral("Registered model type (NodeDelegateModel::name)")}}},
            {QStringLiteral("name"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
            {QStringLiteral("state"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("enum"), QJsonArray{QStringLiteral("running"), QStringLiteral("stopped")}}}}}}});

    schemas.insert(QStringLiteral("NodeList"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("nodes"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")}, {QStringLiteral("items"), refSchema(QStringLiteral("Node"))}}}}}});

    schemas.insert(QStringLiteral("NodeConfig"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("description"), QStringLiteral("Opaque per-node JSON produced by NodeDelegateModel::save(); see /api/nodes/{id}/schema for its shape.")}});

    schemas.insert(QStringLiteral("JsonSchema"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("description"), QStringLiteral("JSON Schema Draft 7 describing a node's configuration.")}});

    schemas.insert(QStringLiteral("Flow"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("nodes"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")}}},
            {QStringLiteral("connections"), QJsonObject{{QStringLiteral("type"), QStringLiteral("array")}}},
            {QStringLiteral("ui"), QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}}},
            {QStringLiteral("path"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("description"), QStringLiteral("Alternative to nodes/connections: server reads the .flow file from disk.")}}}}}});

    schemas.insert(QStringLiteral("MutationResult"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("applied"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
            {QStringLiteral("ok"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
            {QStringLiteral("error"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}}});

    schemas.insert(QStringLiteral("Error"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("error"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}}});

    components.insert(QStringLiteral("schemas"), schemas);

    QJsonObject spec;
    spec.insert(QStringLiteral("openapi"), QStringLiteral("3.0.3"));
    spec.insert(QStringLiteral("info"), info);
    spec.insert(QStringLiteral("servers"), servers);
    spec.insert(QStringLiteral("tags"), QJsonArray{
        QJsonObject{{QStringLiteral("name"), QStringLiteral("health")}},
        QJsonObject{{QStringLiteral("name"), QStringLiteral("flow")}},
        QJsonObject{{QStringLiteral("name"), QStringLiteral("nodes")}},
        QJsonObject{{QStringLiteral("name"), QStringLiteral("events")}},
        QJsonObject{{QStringLiteral("name"), QStringLiteral("meta")}}});
    spec.insert(QStringLiteral("paths"), paths);
    spec.insert(QStringLiteral("components"), components);

    QJsonArray security;
    security.append(QJsonObject{{QStringLiteral("bearerAuth"), QJsonArray()}});
    spec.insert(QStringLiteral("security"), security);

    return spec;
}

// ═══════════════════════════════════════════════════════════════════════════
// WebSocket (RFC 6455)
// ═══════════════════════════════════════════════════════════════════════════

bool ControlPlaneServer::upgradeToWebSocket(QTcpSocket* socket,
                                            const QByteArray& target,
                                            const QHash<QByteArray, QByteArray>& headers)
{
    if (headers.value("sec-websocket-version") != QByteArrayLiteral("13")
        || !headers.value("upgrade").toLower().contains("websocket")) {
        sendBody(socket, 400, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"websocket upgrade required\"}"));
        return false;
    }

    // Browsers cannot set an Authorization header on a WebSocket, so accept
    // the token from the query string as well (REQ-SW-PL-054 AC10/AC11).
    bool authorized = true;
    if (!m_token.isEmpty()) {
        const QUrl url(QString::fromUtf8(target));
        const QByteArray queryToken = QUrlQuery(url).queryItemValue(QStringLiteral("token")).toUtf8();
        const QByteArray headerToken =
            headers.value("authorization").startsWith(QByteArrayLiteral("Bearer "))
                ? headers.value("authorization").mid(7) : QByteArray();
        authorized = constantTimeEquals(queryToken, m_token.toUtf8())
                  || constantTimeEquals(headerToken, m_token.toUtf8());
    }
    if (!authorized) {
        sendBody(socket, 401, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"unauthorized\"}"));
        return false;
    }

    const QByteArray key = headers.value("sec-websocket-key");
    if (key.isEmpty()) {
        sendBody(socket, 400, QByteArrayLiteral("application/json"),
                 QByteArrayLiteral("{\"error\":\"missing Sec-WebSocket-Key\"}"));
        return false;
    }

    QByteArray response;
    response += "HTTP/1.1 101 Switching Protocols\r\n";
    response += "Upgrade: websocket\r\n";
    response += "Connection: Upgrade\r\n";
    response += "Sec-WebSocket-Accept: " + webSocketAccept(key) + "\r\n";
    response += "\r\n";

    socket->write(response);
    socket->flush();

    m_wsClients.insert(socket);
    m_wsBuffers.insert(socket, QByteArray());

    qCInfo(lcApp) << "ControlPlaneServer: WebSocket client connected ("
                  << m_wsClients.size() << "total)";
    return true;
}

void ControlPlaneServer::handleWebSocketData(QTcpSocket* socket)
{
    auto it = m_wsBuffers.find(socket);
    if (it == m_wsBuffers.end())
        return;

    it->append(socket->readAll());

    for (;;) {
        if (it->size() < 2)
            return;

        const quint8 b0 = static_cast<quint8>(it->at(0));
        const quint8 b1 = static_cast<quint8>(it->at(1));
        const quint8 opcode = b0 & 0x0F;
        const bool masked = (b1 & 0x80) != 0;
        quint64 length = b1 & 0x7F;
        int pos = 2;

        if (length == 126) {
            if (it->size() < 4) return;
            length = (static_cast<quint8>(it->at(2)) << 8) | static_cast<quint8>(it->at(3));
            pos = 4;
        } else if (length == 127) {
            if (it->size() < 10) return;
            length = 0;
            for (int i = 0; i < 8; ++i)
                length = (length << 8) | static_cast<quint8>(it->at(2 + i));
            pos = 10;
        }

        if (length > (1u << 20)) { // control API never receives large frames
            dropWebSocketClient(socket);
            return;
        }
        if (masked) {
            if (it->size() < pos + 4) return;
            pos += 4; // masking key: we only need control frames, not the payload
        }
        if (static_cast<quint64>(it->size()) < static_cast<quint64>(pos) + length)
            return;

        const QByteArray payload = it->mid(pos, static_cast<int>(length));
        it->remove(0, pos + static_cast<int>(length));

        switch (opcode) {
        case 0x8: // close
            dropWebSocketClient(socket);
            return;
        case 0x9: // ping -> pong
            sendWebSocketFrame(socket, payload, 0xA);
            break;
        default:
            break; // text/binary/continuation from the client: ignored
        }

        if (!m_wsClients.contains(socket))
            return;
        if (it == m_wsBuffers.end())
            return;
    }
}

void ControlPlaneServer::sendWebSocketFrame(QTcpSocket* socket, const QByteArray& payload,
                                            quint8 opcode)
{
    QByteArray frame;
    frame.append(static_cast<char>(0x80 | (opcode & 0x0F)));
    const quint64 len = static_cast<quint64>(payload.size());
    if (len < 126) {
        frame.append(static_cast<char>(len));
    } else if (len <= 0xFFFF) {
        frame.append(static_cast<char>(126));
        frame.append(static_cast<char>((len >> 8) & 0xFF));
        frame.append(static_cast<char>(len & 0xFF));
    } else {
        frame.append(static_cast<char>(127));
        for (int i = 7; i >= 0; --i)
            frame.append(static_cast<char>((len >> (8 * i)) & 0xFF));
    }
    frame.append(payload);

    socket->write(frame);
    socket->flush();
}

void ControlPlaneServer::dropWebSocketClient(QTcpSocket* socket)
{
    m_wsClients.remove(socket);
    m_wsBuffers.remove(socket);
    socket->disconnectFromHost();
}

void ControlPlaneServer::broadcastEvent(const QString& type, const QJsonObject& data)
{
    if (m_wsClients.isEmpty())
        return;

    QJsonObject message;
    message.insert(QStringLiteral("type"), type);
    message.insert(QStringLiteral("data"), data);
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);

    const QSet<QTcpSocket*> clients = m_wsClients; // copy: sending may drop clients
    for (QTcpSocket* socket : clients) {
        if (socket->state() == QAbstractSocket::ConnectedState)
            sendWebSocketFrame(socket, payload, 0x1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Auth + response writers
// ═══════════════════════════════════════════════════════════════════════════

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

void ControlPlaneServer::sendHtml(QTcpSocket* socket, int status, const QByteArray& html)
{
    sendBody(socket, status, QByteArrayLiteral("text/html; charset=utf-8"), html);
}

void ControlPlaneServer::sendBody(QTcpSocket* socket, int status, const QByteArray& contentType,
                                  const QByteArray& body)
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(status) + ' ' + reasonPhrase(status) + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    // CORS: the bundled web UI and external Web UIs may live on another origin
    // (REQ-SW-PL-054 AC12).
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Headers: Authorization, Content-Type\r\n";
    response += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response += "Cache-Control: no-store\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
}

} // namespace Daqster
