#include "test_llama_console_protocol.h"

#include "ConsoleDataModel.h"
#include "LLamaModelDataModel.h"
#include "NodeDataTypes/TextData.h"

#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSharedPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>

namespace {

std::shared_ptr<TextData> textData(QString const& text)
{
    return std::make_shared<TextData>(text);
}

/// Minimal in-process HTTP stand-in for a llama.cpp server. It answers
/// GET /health and POST /v1/chat/completions so the LLamaModelDataModel
/// network path can be exercised deterministically - no external binary, no
/// GPU and no model file are required. The response shape is configurable so
/// both plain and "thinking" (reasoning_content-only) replies can be tested.
class FakeLlamaServer : public QTcpServer
{
public:
    explicit FakeLlamaServer(QObject* parent = nullptr)
        : QTcpServer(parent)
    {
        connect(this, &QTcpServer::newConnection, this, [this]() { acceptConnections(); });
    }

    /// Assistant message returned by /v1/chat/completions.
    void setAssistantMessage(QJsonObject const& message) { m_assistantMessage = message; }

    QByteArray lastRequestLine() const { return m_lastRequestLine; }
    int chatRequestCount() const { return m_chatRequestCount; }

private:
    void acceptConnections()
    {
        while (QTcpSocket* socket = nextPendingConnection()) {
            // One buffer per socket: QNetworkAccessManager may open several.
            auto buffer = QSharedPointer<QByteArray>::create();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
                buffer->append(socket->readAll());
                const int headerEnd = buffer->indexOf("\r\n\r\n");
                if (headerEnd < 0)
                    return; // wait for the full request head

                const QByteArray head = buffer->left(headerEnd);
                m_lastRequestLine = head.left(head.indexOf("\r\n"));

                QByteArray body;
                if (m_lastRequestLine.startsWith("POST /v1/chat/completions")) {
                    ++m_chatRequestCount;
                    QJsonArray choices;
                    choices.append(QJsonObject{{QStringLiteral("message"), m_assistantMessage}});
                    QJsonObject response;
                    response[QStringLiteral("choices")] = choices;
                    body = QJsonDocument(response).toJson(QJsonDocument::Compact);
                } else {
                    body = QByteArrayLiteral("{\"status\":\"ok\"}");
                }

                socket->write("HTTP/1.1 200 OK\r\n"
                              "Content-Type: application/json\r\n"
                              "Connection: close\r\n"
                              "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n");
                socket->write(body);
                socket->flush();
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    }

    QJsonObject m_assistantMessage;
    QByteArray m_lastRequestLine;
    int m_chatRequestCount = 0;
};

/// Writes a POSIX shell "server" whose ONLY job is to emit the readiness
/// marker on stderr - exactly where llama.cpp writes "server is listening".
/// It then stays alive so the node keeps its connection.
QString writeFakeServerScript(QTemporaryDir& dir, int port)
{
    const QString path = dir.filePath(QStringLiteral("fake_llama_server.sh"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();

    file.write("#!/bin/sh\n"
               "echo \"main: server is listening on http://127.0.0.1:");
    file.write(QByteArray::number(port));
    file.write(" - starting the main loop\" >&2\n"
               "sleep 60\n");
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

QJsonObject localServerConfig(QString const& script, int port)
{
    return QJsonObject{{QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                       {QStringLiteral("port"), port},
                       {QStringLiteral("exePath"), script},
                       {QStringLiteral("modelPath"), QStringLiteral("dummy.gguf")},
                       {QStringLiteral("ctxSize"), 128},
                       {QStringLiteral("useGpu"), false}};
}

QJsonObject consoleRequestEnvelope(QString const& prompt)
{
    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), prompt}});
    QJsonObject envelope;
    envelope[QStringLiteral("messages")] = messages;
    envelope[QStringLiteral("temperature")] = 0.7;
    envelope[QStringLiteral("n_predict")] = 64;
    return envelope;
}

QJsonObject llamaModelResponseEnvelope(QString const& content)
{
    QJsonObject choice;
    choice[QStringLiteral("message")] = QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                                    {QStringLiteral("content"), content}};
    QJsonArray choices;
    choices.append(choice);

    QJsonObject response;
    response[QStringLiteral("choices")] = choices;

    QJsonObject envelope;
    envelope[QStringLiteral("request_body")] = consoleRequestEnvelope(QStringLiteral("ping"));
    envelope[QStringLiteral("response")] = response;
    return envelope;
}

QString compact(QJsonObject const& obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace

// QtNodes::PortIndex is a plain typedef in the signal signature of
// dataUpdated; QSignalSpy needs it registered to record the argument cleanly
// (otherwise Qt logs "Unable to handle parameter 'index'").
void TestLlamaConsoleProtocol::initTestCase()
{
    qRegisterMetaType<QtNodes::PortIndex>("PortIndex");
}

// AC 2: what the Конзола puts on its output port is exactly what LlamaModel's
// setInData() reads back.
void TestLlamaConsoleProtocol::consoleEmitsRequestEnvelopeWithModelSchema()
{
    ConsoleDataModel console;
    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), QStringLiteral("hello")}});

    console.onSendClicked(QStringLiteral("hello"), messages, 0.7, 64);

    auto data = std::dynamic_pointer_cast<TextData>(console.outData(0));
    QVERIFY(data != nullptr);

    QJsonDocument doc = QJsonDocument::fromJson(data->text().toUtf8());
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();

    QVERIFY(obj.contains(QStringLiteral("messages")));
    QVERIFY(obj.contains(QStringLiteral("temperature")));
    QVERIFY(obj.contains(QStringLiteral("n_predict")));
    QCOMPARE(obj.value(QStringLiteral("messages")).toArray().size(), 1);
    QCOMPARE(obj.value(QStringLiteral("temperature")).toDouble(), 0.7);
    QCOMPARE(obj.value(QStringLiteral("n_predict")).toInt(), 64);
}

// AC 2: LlamaModel parses the Конзола envelope. With no server connection the
// request cannot be served, but parsing must not crash and the model must not
// silently treat the input as junk.
void TestLlamaConsoleProtocol::llamaModelAcceptsConsoleEnvelope()
{
    LLamaModelDataModel model;
    QSignalSpy statusSpy(&model, &LLamaModelDataModel::statusChanged);

    model.setInData(textData(compact(consoleRequestEnvelope(QStringLiteral("ping")))), 0);

    // No connection → one diagnostic status (AC 4), not a silent drop.
    QVERIFY(statusSpy.count() >= 1);
}

// AC 4: a request that cannot be served must say so.
void TestLlamaConsoleProtocol::llamaModelReportsNoConnectionInsteadOfSilence()
{
    LLamaModelDataModel model;
    QSignalSpy statusSpy(&model, &LLamaModelDataModel::statusChanged);

    model.setInData(textData(compact(consoleRequestEnvelope(QStringLiteral("ping")))), 0);

    QCOMPARE(statusSpy.count(), 1);
    const QString status = statusSpy.first().at(0).toString();
    QVERIFY2(!status.isEmpty(), "the no-connection status must carry a message");
}

// AC 3: what LlamaModel publishes, the Конзола reads back into a reply.
void TestLlamaConsoleProtocol::consoleExtractsReplyFromLlamaModelEnvelope()
{
    ConsoleDataModel console;
    QSignalSpy responseSpy(&console, &ConsoleDataModel::responseReceived);

    console.setInData(textData(compact(llamaModelResponseEnvelope(QStringLiteral("pong")))), 0);

    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(responseSpy.first().at(0).toString(), QStringLiteral("pong"));
    // The raw envelope rides along for downstream display.
    QVERIFY(responseSpy.first().at(1).toJsonObject().contains(QStringLiteral("response")));
}

// The Конзола also tolerates the slimmer {"message":{...}} / {"content":...}
// shapes (legacy / debug responses).
void TestLlamaConsoleProtocol::consoleAlsoAcceptsMessageAndContentShapes()
{
    {
        ConsoleDataModel console;
        QSignalSpy spy(&console, &ConsoleDataModel::responseReceived);
        QJsonObject obj{{QStringLiteral("message"),
                         QJsonObject{{QStringLiteral("content"), QStringLiteral("from-message")}}}};
        console.setInData(textData(compact(obj)), 0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("from-message"));
    }
    {
        ConsoleDataModel console;
        QSignalSpy spy(&console, &ConsoleDataModel::responseReceived);
        QJsonObject obj{{QStringLiteral("content"), QStringLiteral("from-content")}};
        console.setInData(textData(compact(obj)), 0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("from-content"));
    }
}

// REQ-SW-PL-074 (R6): thinking models (Qwen3/QwQ/DeepSeek-R1) leave
// message.content EMPTY and put the text in reasoning_content. The Конзола
// must still surface a reply - otherwise the crosswise flow looks dead.
void TestLlamaConsoleProtocol::consoleExtractsReplyFromThinkingModelEnvelope()
{
    ConsoleDataModel console;
    QSignalSpy responseSpy(&console, &ConsoleDataModel::responseReceived);

    QJsonObject message{{QStringLiteral("role"), QStringLiteral("assistant")},
                        {QStringLiteral("content"), QString()},
                        {QStringLiteral("reasoning_content"), QStringLiteral("thought-pong")}};
    QJsonArray choices;
    choices.append(QJsonObject{{QStringLiteral("message"), message}});
    QJsonObject response;
    response[QStringLiteral("choices")] = choices;
    QJsonObject envelope;
    envelope[QStringLiteral("response")] = response;

    console.setInData(textData(compact(envelope)), 0);

    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(responseSpy.first().at(0).toString(), QStringLiteral("thought-pong"));
}

void TestLlamaConsoleProtocol::llamaModelIgnoresEmptyAndMalformedInput()
{
    LLamaModelDataModel model;

    {
        QSignalSpy statusSpy(&model, &LLamaModelDataModel::statusChanged);
        model.setInData(nullptr, 0);
        model.setInData(textData(QString()), 0);
        model.setInData(textData(QStringLiteral("not json at all")), 0);
        model.setInData(textData(QStringLiteral("{\"messages\":[]}")), 0);
        QCOMPARE(statusSpy.count(), 0);
    }
}

// REQ-SW-PL-074 (R5/R6): starting the node's own local server must actually
// make the node able to talk to it. llama.cpp writes its "server is listening"
// marker to stderr, so a stdout-only base-URL assignment left m_baseUrl empty
// and every request POSTed to a relative URL and failed silently. And a
// "thinking" model returns the text in reasoning_content with an empty
// content, which the node used to drop.
void TestLlamaConsoleProtocol::localServerStartEstablishesBaseUrlAndDeliversReply()
{
#ifdef Q_OS_WIN
    QSKIP("The fake llama-server stand-in is a POSIX shell script.");
#else
    FakeLlamaServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    server.setAssistantMessage(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("assistant")},
        {QStringLiteral("content"), QString()},
        {QStringLiteral("reasoning_content"), QStringLiteral("pong")}});

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString script = writeFakeServerScript(dir, server.serverPort());
    QVERIFY(!script.isEmpty());

    LLamaModelDataModel model;
    model.load(localServerConfig(script, server.serverPort()));

    QSignalSpy replySpy(&model, &LLamaModelDataModel::chatResponseReceived);

    model.onStartServerClicked();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 15000);

    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), QStringLiteral("ping")}});
    model.onLocalChatSend(QStringLiteral("ping"), messages, 0.2, 16);

    QTRY_VERIFY_WITH_TIMEOUT(replySpy.count() >= 1, 15000);
    QCOMPARE(replySpy.first().at(0).toString(), QStringLiteral("pong"));
    QVERIFY(server.chatRequestCount() >= 1);
    QVERIFY(server.lastRequestLine().startsWith("POST /v1/chat/completions"));

    model.stop();
#endif
}

// REQ-SW-PL-074 (AC 2-4): the full crosswise contract over a live connection -
// the Конзола envelope goes in, the model's output envelope comes out and the
// Конзола reads the reply back out of it.
void TestLlamaConsoleProtocol::consoleEnvelopeReachesLocalServerAndReturnsExchange()
{
#ifdef Q_OS_WIN
    QSKIP("The fake llama-server stand-in is a POSIX shell script.");
#else
    FakeLlamaServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    server.setAssistantMessage(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                           {QStringLiteral("content"), QString()},
                                           {QStringLiteral("reasoning_content"), QStringLiteral("pong")}});

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString script = writeFakeServerScript(dir, server.serverPort());
    QVERIFY(!script.isEmpty());

    LLamaModelDataModel model;
    model.load(localServerConfig(script, server.serverPort()));

    QSignalSpy exchangeSpy(&model, &LLamaModelDataModel::exchangeAppended);
    QSignalSpy dataSpy(&model, &LLamaModelDataModel::dataUpdated);

    model.onStartServerClicked();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 15000);

    model.setInData(textData(compact(consoleRequestEnvelope(QStringLiteral("ping")))), 0);

    QTRY_VERIFY_WITH_TIMEOUT(exchangeSpy.count() >= 1, 15000);
    QVERIFY(dataSpy.count() >= 1);

    // What the model published on its output port, the Конзола must read back.
    ConsoleDataModel console;
    QSignalSpy consoleSpy(&console, &ConsoleDataModel::responseReceived);
    console.setInData(model.outData(0), 0);
    QCOMPARE(consoleSpy.count(), 1);
    QCOMPARE(consoleSpy.first().at(0).toString(), QStringLiteral("pong"));

    model.stop();
#endif
}

// Opt-in live integration against a REAL llama.cpp server (env
// LLAMA_SERVER_BIN + LLAMA_SERVER_MODEL). It reproduces the user's exact
// scenario - the node starts its own server - and proves the full path:
// launch -> readiness marker on stderr -> base URL -> POST -> reply parsed.
// A thinking model (Qwen3) is the harshest case: the reply only materialises
// through the reasoning_content fallback.
void TestLlamaConsoleProtocol::liveRealServerEndToEnd()
{
#ifdef Q_OS_WIN
    QSKIP("Live llama.cpp integration runs on Linux only.");
#else
    const QByteArray bin = qgetenv("LLAMA_SERVER_BIN");
    const QByteArray modelFile = qgetenv("LLAMA_SERVER_MODEL");
    if (bin.isEmpty() || modelFile.isEmpty())
        QSKIP("LLAMA_SERVER_BIN / LLAMA_SERVER_MODEL not set - live test skipped");

    LLamaModelDataModel model;
    model.load(QJsonObject{{QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                           {QStringLiteral("port"), 18095},
                           {QStringLiteral("exePath"), QString::fromLocal8Bit(bin)},
                           {QStringLiteral("modelPath"), QString::fromLocal8Bit(modelFile)},
                           {QStringLiteral("ctxSize"), 512},
                           {QStringLiteral("useGpu"), false}});

    QSignalSpy exchangeSpy(&model, &LLamaModelDataModel::exchangeAppended);
    QSignalSpy replySpy(&model, &LLamaModelDataModel::chatResponseReceived);

    model.onStartServerClicked();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 240000);

    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), QStringLiteral("hi")}});
    model.onLocalChatSend(QStringLiteral("hi"), messages, 0.1, 24);

    QTRY_VERIFY_WITH_TIMEOUT(exchangeSpy.count() >= 1, 180000);
    QVERIFY(replySpy.count() >= 1);
    QVERIFY(!replySpy.first().at(0).toString().isEmpty());

    model.stop();
#endif
}

QTEST_GUILESS_MAIN(TestLlamaConsoleProtocol)