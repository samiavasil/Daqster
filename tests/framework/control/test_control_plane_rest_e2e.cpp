// End-to-end test for the REST control plane over a real HTTP socket
// (REQ-SW-PL-063 / REQ-SW-PL-091).
//
// The unit tests drive `ControlPlaneRouter::route()` directly, so they cannot
// see the parts that only exist once a host is running: framing, `setNodeConfig`
// against a live model, and the `started`/`restartRequired` pair that a config
// write reports. This test starts the real `NodeRunner --headless --rest-port`
// process, drives it over TCP and asserts the sequence
// `POST /api/flow` -> `GET /api/nodes` -> `PATCH` config -> `POST .../start`
// -> `POST /api/flow/close`.
//
// Opt-in only: `DAQSTER_REST_E2E=1`. Without it the test is a `QSKIP`, so the
// normal `ctest` run pays nothing for the process spawn.

#include <QtTest>

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

namespace {

/// One HTTP reply, parsed far enough for JSON assertions.
struct HttpResponse
{
    int status = 0;
    QJsonObject body;
};

/// Ask the kernel for a port nobody is listening on right now.
quint16 freePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    return probe.serverPort();
}

/// Send one request to 127.0.0.1:@p port and read the reply to EOF.
///
/// The control plane closes the socket after every plain-HTTP response
/// (`ControlPlaneServer::dispatch()`), so "read until disconnected" terminates.
bool httpRequest(quint16 port, const QByteArray& method, const QByteArray& path,
                 const QJsonObject& body, HttpResponse* out, QString* error)
{
    QByteArray payload;
    if (!body.isEmpty())
        payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QByteArray request;
    request += method + ' ' + path + " HTTP/1.1\r\n";
    request += "Host: 127.0.0.1\r\n";
    request += "Connection: close\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n\r\n";
    request += payload;

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    if (!socket.waitForConnected(5000)) {
        *error = QStringLiteral("cannot connect to 127.0.0.1:%1: %2")
                     .arg(port)
                     .arg(socket.errorString());
        return false;
    }
    if (socket.write(request) != request.size() || !socket.waitForBytesWritten(5000)) {
        *error = QStringLiteral("cannot write the request");
        return false;
    }

    QByteArray raw;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        if (socket.waitForReadyRead(2000))
            raw += socket.readAll();
        if (socket.state() == QAbstractSocket::UnconnectedState) {
            raw += socket.readAll();
            break;
        }
    }
    if (raw.isEmpty()) {
        *error = QStringLiteral("empty reply for %1 %2")
                     .arg(QString::fromUtf8(method), QString::fromUtf8(path));
        return false;
    }

    const int headerEnd = raw.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        *error = QStringLiteral("malformed reply for %1")
                     .arg(QString::fromUtf8(path));
        return false;
    }
    const QByteArray statusLine = raw.left(raw.indexOf("\r\n"));
    const QList<QByteArray> statusParts = statusLine.split(' ');
    if (statusParts.size() < 2) {
        *error = QStringLiteral("no status code in %1").arg(QString::fromUtf8(statusLine));
        return false;
    }
    out->status = statusParts.at(1).toInt();
    const QByteArray bodyRaw = raw.mid(headerEnd + 4);
    const QJsonDocument doc = QJsonDocument::fromJson(bodyRaw);
    out->body = doc.isObject() ? doc.object() : QJsonObject();
    return true;
}

/// A `NodeRunner --headless --rest-port <p>` child that is always killed.
///
/// Nothing here writes to the graph outside the API: the process is the system
/// under test, not a fixture we poke at.
class RestHost
{
public:
    ~RestHost() { stop(); }

    bool start(quint16 port, QString* error)
    {
        const QString runner = QCoreApplication::applicationDirPath()
                               + QStringLiteral("/NodeRunner");
        if (!QFileInfo::exists(runner)) {
            *error = QStringLiteral("%1 does not exist").arg(runner);
            return false;
        }

        QJsonObject scene = sceneJson();
        if (!m_sceneDir.isValid()) {
            *error = QStringLiteral("cannot create the temporary scene directory");
            return false;
        }
        const QString scenePath = m_sceneDir.filePath(QStringLiteral("rest_e2e.flow"));
        QFile file(scenePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            *error = QStringLiteral("cannot write %1").arg(scenePath);
            return false;
        }
        file.write(QJsonDocument(scene).toJson(QJsonDocument::Compact));
        file.close();

        m_process.setProcessChannelMode(QProcess::MergedChannels);
        m_process.start(runner, {QStringLiteral("--headless"),
                                 QStringLiteral("--run"), scenePath,
                                 QStringLiteral("--rest-port"), QString::number(port)});
        if (!m_process.waitForStarted(15000)) {
            *error = QStringLiteral("NodeRunner did not start: %1").arg(m_process.errorString());
            return false;
        }
        m_port = port;

        // /api/health is the readiness contract of the control plane.
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 20000) {
            if (m_process.state() == QProcess::NotRunning) {
                *error = QStringLiteral("NodeRunner exited during startup: %1")
                             .arg(consoleLog());
                return false;
            }
            HttpResponse health;
            QString ignored;
            if (httpRequest(port, QByteArrayLiteral("GET"), QByteArrayLiteral("/api/health"),
                            {}, &health, &ignored)
                && health.status == 200) {
                return true;
            }
            QTest::qWait(150);
        }
        *error = QStringLiteral("control plane never became ready: %1").arg(consoleLog());
        return false;
    }

    void stop()
    {
        if (m_process.state() == QProcess::NotRunning)
            return;
        m_process.kill();
        if (!m_process.waitForFinished(5000))
            qWarning("RestHost: NodeRunner did not die in 5 s");
    }

    bool running() const { return m_process.state() != QProcess::NotRunning; }
    quint16 port() const { return m_port; }

    QString consoleLog() const
    {
        return QString::fromUtf8(const_cast<QProcess&>(m_process).readAll()).right(2000);
    }

    /// The scene the test loads through `POST /api/flow`.
    ///
    /// Self-contained on purpose: `NumberSource -> NumberResult` (the public
    /// pair that exists in headless mode) plus `SystemMonitor`, the lightest
    /// public `IStartable` node, so `POST /api/nodes/{id}/start` and the
    /// `restartRequired` branch are covered without any device or file.
    static QJsonObject sceneJson()
    {
        auto node = [](int id, const QString& type, const QJsonObject& data,
                       double x, double y) {
            QJsonObject internal = data;
            internal.insert(QStringLiteral("model-name"), type);
            QJsonObject position;
            position.insert(QStringLiteral("x"), x);
            position.insert(QStringLiteral("y"), y);
            return QJsonObject{{QStringLiteral("id"), id},
                               {QStringLiteral("internal-data"), internal},
                               {QStringLiteral("label"), type},
                               {QStringLiteral("labelVisible"), true},
                               {QStringLiteral("position"), position}};
        };

        QJsonObject sourceData{{QStringLiteral("type"), QStringLiteral("double")},
                               {QStringLiteral("randomEnabled"), false},
                               {QStringLiteral("interval"), 1000},
                               {QStringLiteral("number"), QStringLiteral("42")}};
        QJsonObject resultData{{QStringLiteral("name"), QStringLiteral("NumberResult")}};
        QJsonObject monitorData{{QStringLiteral("pollIntervalSec"), 1.0}};

        QJsonObject connection{{QStringLiteral("outNodeId"), 0},
                               {QStringLiteral("outPortIndex"), 0},
                               {QStringLiteral("inNodeId"), 1},
                               {QStringLiteral("inPortIndex"), 0}};

        return QJsonObject{
            {QStringLiteral("nodes"), QJsonArray{node(0, QStringLiteral("NumberSource"), sourceData, -200.0, 0.0),
                                                 node(1, QStringLiteral("NumberResult"), resultData, 0.0, 0.0),
                                                 node(2, QStringLiteral("SystemMonitor"), monitorData, 0.0, 150.0)}},
            {QStringLiteral("connections"), QJsonArray{connection}}};
    }

private:
    QProcess m_process;
    QTemporaryDir m_sceneDir;
    quint16 m_port = 0;
};

/// Find the id of the single node of @p type in `GET /api/nodes`.
int nodeIdOfType(const QJsonObject& nodesReply, const QString& type)
{
    const QJsonArray nodes = nodesReply.value(QStringLiteral("nodes")).toArray();
    for (const QJsonValue& value : nodes) {
        const QJsonObject node = value.toObject();
        if (node.value(QStringLiteral("type")).toString() == type)
            return node.value(QStringLiteral("id")).toInt(-1);
    }
    return -1;
}

} // namespace

class ControlPlaneRestE2ETest : public QObject
{
    Q_OBJECT

private slots:
    void configWriteOverRestReportsRestartNeed()
    {
        if (qEnvironmentVariableIsEmpty("DAQSTER_REST_E2E"))
            QSKIP("задай DAQSTER_REST_E2E=1");
        if (!QFileInfo::exists(QCoreApplication::applicationDirPath()
                               + QStringLiteral("/NodeRunner")))
            QSKIP("NodeRunner не е до built binaries — този тест иска пълен билд");

        RestHost host;
        QString error;
        // A port freed a moment ago can be taken in between; retry, never hang.
        bool started = false;
        for (int attempt = 0; attempt < 3 && !started; ++attempt) {
            const quint16 port = freePort();
            QVERIFY2(port != 0, "не мога да намеря свободен порт");
            started = host.start(port, &error);
            if (!started)
                host.stop();
        }
        QVERIFY2(started, qPrintable(error));

        // ── 1. Load the scene over the API (not through --run). ─────────────
        HttpResponse reply;
        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("POST"), QByteArrayLiteral("/api/flow"),
                             RestHost::sceneJson(), &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        QCOMPARE(reply.body.value(QStringLiteral("applied")).toBool(), true);

        // ── 2. Both kinds of node are there. ────────────────────────────────
        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("GET"), QByteArrayLiteral("/api/nodes"),
                             {}, &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        QCOMPARE(reply.body.value(QStringLiteral("nodes")).toArray().size(), 3);
        const int sourceId = nodeIdOfType(reply.body, QStringLiteral("NumberSource"));
        const int resultId = nodeIdOfType(reply.body, QStringLiteral("NumberResult"));
        const int monitorId = nodeIdOfType(reply.body, QStringLiteral("SystemMonitor"));
        QVERIFY(sourceId >= 0);
        QVERIFY(resultId >= 0);
        QVERIFY(monitorId >= 0);

        // ── 3. PATCH config on a stopped node: applied, no restart needed. ──
        QJsonObject patch;
        patch.insert(QStringLiteral("config"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("int")},
                                 {QStringLiteral("randomEnabled"), false},
                                 {QStringLiteral("interval"), 500},
                                 {QStringLiteral("number"), QStringLiteral("7")}});
        const QByteArray sourcePath = "/api/nodes/" + QByteArray::number(sourceId);
        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("PATCH"), sourcePath,
                             patch, &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        QCOMPARE(reply.body.value(QStringLiteral("applied")).toBool(), true);
        QCOMPARE(reply.body.value(QStringLiteral("started")).toBool(), false);
        QCOMPARE(reply.body.value(QStringLiteral("restartRequired")).toBool(), false);

        // The write really landed on the live model.
        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("GET"),
                             sourcePath + "/config", {}, &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        QCOMPARE(reply.body.value(QStringLiteral("type")).toString(), QStringLiteral("int"));
        QCOMPARE(reply.body.value(QStringLiteral("interval")).toInt(), 500);

        // ── 4. Start a node, then PATCH it: a live field needs no restart. ──
        const QByteArray monitorPath = "/api/nodes/" + QByteArray::number(monitorId);
        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("GET"), monitorPath,
                             {}, &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        // Both hosts report whether the node can be started at all; the router
        // needs it for a changed key the schema does not mark (REQ-SW-PL-091).
        QCOMPARE(reply.body.value(QStringLiteral("startable")).toBool(), true);
        QCOMPARE(reply.body.value(QStringLiteral("schema"))
                     .toObject().value(QStringLiteral("properties")).toObject()
                     .value(QStringLiteral("pollIntervalSec")).toObject()
                     .value(QStringLiteral("x-restart")).toBool(false),
                 false);

        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("POST"),
                             monitorPath + "/start", {}, &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        QCOMPARE(reply.body.value(QStringLiteral("ok")).toBool(), true);

        // pollIntervalSec goes straight into the running poll timer
        // (SystemMonitorModel::configSchema marks it x-restart:false), so the
        // write is complete: no stop()/start() cycle to ask for.
        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("PATCH"), monitorPath,
                             QJsonObject{{QStringLiteral("config"),
                                          QJsonObject{{QStringLiteral("pollIntervalSec"), 2.0}}}},
                             &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        QCOMPARE(reply.body.value(QStringLiteral("applied")).toBool(), true);
        QCOMPARE(reply.body.value(QStringLiteral("started")).toBool(), true);
        QCOMPARE(reply.body.value(QStringLiteral("restartRequired")).toBool(), false);

        // Writing back the value the node already has is not a change either.
        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("POST"),
                             monitorPath + "/config",
                             QJsonObject{{QStringLiteral("pollIntervalSec"), 2.0}},
                             &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        QCOMPARE(reply.body.value(QStringLiteral("started")).toBool(), true);
        QCOMPARE(reply.body.value(QStringLiteral("restartRequired")).toBool(), false);

        // ── 5. Close the scene through the API, then let the host die. ───────
        QVERIFY2(httpRequest(host.port(), QByteArrayLiteral("POST"),
                             QByteArrayLiteral("/api/flow/close"), {}, &reply, &error),
                 qPrintable(error));
        QCOMPARE(reply.status, 200);
        QCOMPARE(reply.body.value(QStringLiteral("closed")).toBool(), true);

        host.stop();
        QVERIFY2(!host.running(), "NodeRunner остана жив след теста");
    }
};

QTEST_GUILESS_MAIN(ControlPlaneRestE2ETest)
#include "test_control_plane_rest_e2e.moc"
