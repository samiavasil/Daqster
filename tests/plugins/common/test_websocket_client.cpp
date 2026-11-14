// SPDX-License-Identifier: MIT
//
// Tests for Daqster::WebSocketClient (REQ-SW-PL-087), against a tiny in-test
// RFC 6455 server. The server is deliberately dumb: it performs the upgrade,
// records whether client frames were masked, echoes text/binary, records
// pongs/close codes and can forge fragmentation, ping and oversized frames.

#include "test_websocket_client.h"

#include "WebSocket/WebSocketClient.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

namespace {

/// Minimal WebSocket server: handshake + frame echo/record, no Qt WebSockets.
class WsTestServer : public QObject
{
public:
    enum class AcceptMode { Correct, Wrong };

    explicit WsTestServer(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* socket = m_server.nextPendingConnection();
            if (m_abortOnConnect) {
                socket->abort();
                socket->deleteLater();
                return;
            }
            m_client = socket;
            connect(socket, &QTcpSocket::readyRead, this, [this] { onReadyRead(); });
        });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }
    void setAcceptMode(AcceptMode mode) { m_acceptMode = mode; }
    void setAbortOnConnect(bool abort) { m_abortOnConnect = abort; }
    QTcpSocket* client() const { return m_client; }

    bool handshakeOk = false;
    bool sawMasked = false;
    bool sawUnmasked = false;
    bool sawClose = false;
    QByteArray lastPong;
    int lastCloseCode = -1;

    void sendFrame(quint8 opcode, const QByteArray& payload, bool fin = true)
    {
        if (!m_client)
            return;
        QByteArray frame;
        frame.append(static_cast<char>((fin ? 0x80 : 0x00) | (opcode & 0x0F)));
        const quint64 length = static_cast<quint64>(payload.size());
        if (length < 126) {
            frame.append(static_cast<char>(length));
        } else if (length <= 0xFFFF) {
            frame.append(static_cast<char>(126));
            frame.append(static_cast<char>((length >> 8) & 0xFF));
            frame.append(static_cast<char>(length & 0xFF));
        } else {
            frame.append(static_cast<char>(127));
            for (int i = 7; i >= 0; --i)
                frame.append(static_cast<char>((length >> (8 * i)) & 0xFF));
        }
        frame.append(payload);
        m_client->write(frame);
    }

    void sendFragmentedText(const QByteArray& whole, int chunk)
    {
        for (int pos = 0; pos < whole.size(); pos += chunk) {
            const bool first = pos == 0;
            const bool last = pos + chunk >= whole.size();
            sendFrame(first ? 0x1 : 0x0, whole.mid(pos, chunk), last);
        }
    }

private:
    void onReadyRead()
    {
        if (!m_client)
            return;
        m_buffer += m_client->readAll();

        if (!m_handshaken) {
            const int end = m_buffer.indexOf("\r\n\r\n");
            if (end < 0)
                return;
            const QByteArray head = m_buffer.left(end);
            m_buffer.remove(0, end + 4);
            QByteArray key;
            for (const QByteArray& line : head.split('\n')) {
                if (line.toLower().startsWith("sec-websocket-key:"))
                    key = line.mid(line.indexOf(':') + 1).trimmed();
            }
            handshakeOk = !key.isEmpty();
            const QByteArray accept =
                m_acceptMode == AcceptMode::Correct
                    ? QCryptographicHash::hash(
                          key + QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"),
                          QCryptographicHash::Sha1)
                          .toBase64()
                    : QByteArrayLiteral("bm90LXRoZS1yaWdodC1hbnN3ZXI=");
            m_client->write("HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: " + accept + "\r\n\r\n");
            m_handshaken = true;
        }

        while (m_buffer.size() >= 2) {
            const quint8 b0 = static_cast<quint8>(m_buffer.at(0));
            const quint8 b1 = static_cast<quint8>(m_buffer.at(1));
            const quint8 opcode = b0 & 0x0F;
            const bool masked = (b1 & 0x80) != 0;
            if (masked)
                sawMasked = true;
            else
                sawUnmasked = true;

            quint64 length = b1 & 0x7F;
            int header = 2;
            if (length == 126) {
                if (m_buffer.size() < 4)
                    return;
                length = (static_cast<quint8>(m_buffer.at(2)) << 8)
                         | static_cast<quint8>(m_buffer.at(3));
                header = 4;
            } else if (length == 127) {
                if (m_buffer.size() < 10)
                    return;
                length = 0;
                for (int i = 0; i < 8; ++i)
                    length = (length << 8) | static_cast<quint8>(m_buffer.at(2 + i));
                header = 10;
            }
            const int maskSize = masked ? 4 : 0;
            if (static_cast<quint64>(m_buffer.size())
                < static_cast<quint64>(header + maskSize) + length)
                return;

            const QByteArray mask = masked ? m_buffer.mid(header, 4) : QByteArray();
            QByteArray payload = m_buffer.mid(header + maskSize, static_cast<int>(length));
            m_buffer.remove(0, header + maskSize + static_cast<int>(length));
            if (masked) {
                for (int i = 0; i < payload.size(); ++i)
                    payload[i] = static_cast<char>(payload.at(i) ^ mask.at(i % 4));
            }

            switch (opcode) {
            case 0x1:
                sendFrame(0x1, payload);
                break;
            case 0x2:
                sendFrame(0x2, payload);
                break;
            case 0x8: {
                sawClose = true;
                lastCloseCode = payload.size() >= 2
                                    ? ((static_cast<quint8>(payload.at(0)) << 8)
                                       | static_cast<quint8>(payload.at(1)))
                                    : 1005;
                sendFrame(0x8, payload.left(2));
                break;
            }
            case 0xA:
                lastPong = payload;
                break;
            default:
                break;
            }
        }
    }

    QTcpServer m_server;
    QTcpSocket* m_client = nullptr;
    QByteArray m_buffer;
    AcceptMode m_acceptMode = AcceptMode::Correct;
    bool m_abortOnConnect = false;
    bool m_handshaken = false;
};

QUrl wsUrl(const WsTestServer& server, const QString& scheme = QStringLiteral("ws"))
{
    return QUrl(QStringLiteral("%1://127.0.0.1:%2/socket").arg(scheme).arg(server.port()));
}

} // namespace

void TestWebSocketClient::handshakeValidatesAccept()
{
    WsTestServer server;
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    QSignalSpy errors(&client, &Daqster::WebSocketClient::errorOccurred);

    client.open(wsUrl(server));
    QVERIFY2(connected.wait(5000), qPrintable(errors.isEmpty()
                                                  ? QStringLiteral("no connected()")
                                                  : errors.at(0).at(0).toString()));
    QVERIFY(client.isOpen());
    QVERIFY(server.handshakeOk);
}

void TestWebSocketClient::clientFramesAreMasked()
{
    WsTestServer server;
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    client.open(wsUrl(server));
    QVERIFY(connected.wait(5000));

    QSignalSpy echoed(&client, &Daqster::WebSocketClient::textMessage);
    client.sendText(QStringLiteral("masked?"));
    QVERIFY(echoed.wait(5000));
    QVERIFY(server.sawMasked);
    QVERIFY(!server.sawUnmasked);
}

void TestWebSocketClient::textRoundTrip()
{
    WsTestServer server;
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    client.open(wsUrl(server));
    QVERIFY(connected.wait(5000));

    const QString text = QString::fromUtf8("здравей, свят");
    QSignalSpy messages(&client, &Daqster::WebSocketClient::textMessage);
    client.sendText(text);
    QVERIFY(messages.wait(5000));
    QCOMPARE(messages.at(0).at(0).toString(), text);
}

void TestWebSocketClient::binaryRoundTrip()
{
    WsTestServer server;
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    client.open(wsUrl(server));
    QVERIFY(connected.wait(5000));

    QByteArray payload;
    for (int i = 0; i < 300; ++i)
        payload.append(static_cast<char>(i % 256));

    QSignalSpy messages(&client, &Daqster::WebSocketClient::binaryMessage);
    client.sendBinary(payload);
    QVERIFY(messages.wait(5000));
    QCOMPARE(messages.at(0).at(0).toByteArray(), payload);
}

void TestWebSocketClient::fragmentedMessageIsReassembled()
{
    WsTestServer server;
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    client.open(wsUrl(server));
    QVERIFY(connected.wait(5000));

    QSignalSpy messages(&client, &Daqster::WebSocketClient::textMessage);
    server.sendFragmentedText(QByteArrayLiteral("abcdefghij"), 3);
    QVERIFY(messages.wait(5000));
    QCOMPARE(messages.count(), 1);
    QCOMPARE(messages.at(0).at(0).toString(), QStringLiteral("abcdefghij"));
}

void TestWebSocketClient::pingIsAnsweredWithPong()
{
    WsTestServer server;
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    client.open(wsUrl(server));
    QVERIFY(connected.wait(5000));

    server.sendFrame(0x9, QByteArrayLiteral("abc"));
    QTRY_COMPARE(server.lastPong, QByteArrayLiteral("abc"));

    QSignalSpy messages(&client, &Daqster::WebSocketClient::textMessage);
    client.sendText(QStringLiteral("still alive"));
    QVERIFY(messages.wait(5000));
    QCOMPARE(messages.at(0).at(0).toString(), QStringLiteral("still alive"));
}

void TestWebSocketClient::closeCodeRoundTrip()
{
    WsTestServer server;
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    client.open(wsUrl(server));
    QVERIFY(connected.wait(5000));

    QSignalSpy disconnected(&client, &Daqster::WebSocketClient::disconnected);
    client.close(4001);
    QVERIFY(disconnected.wait(5000));
    QCOMPARE(disconnected.at(0).at(0).toInt(), 4001);
    QCOMPARE(server.lastCloseCode, 4001);
    QVERIFY(!client.isOpen());
}

void TestWebSocketClient::invalidAcceptIsRejected()
{
    WsTestServer server;
    server.setAcceptMode(WsTestServer::AcceptMode::Wrong);
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    QSignalSpy errors(&client, &Daqster::WebSocketClient::errorOccurred);

    client.open(wsUrl(server));
    QVERIFY(errors.wait(5000));
    QCOMPARE(connected.count(), 0);
    QVERIFY(!client.isOpen());
    QVERIFY(errors.at(0).at(0).toString().contains(QStringLiteral("Accept")));
}

void TestWebSocketClient::oversizedMessageIsRejected()
{
    WsTestServer server;
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    client.setMaxMessageSize(1024);
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    client.open(wsUrl(server));
    QVERIFY(connected.wait(5000));

    QSignalSpy errors(&client, &Daqster::WebSocketClient::errorOccurred);
    QSignalSpy disconnected(&client, &Daqster::WebSocketClient::disconnected);
    server.sendFrame(0x1, QByteArray(2048, 'a'));

    QVERIFY(errors.wait(5000));
    QVERIFY(errors.at(0).at(0).toString().contains(QStringLiteral("по-голямо")));
    // The failure tears the connection down in the same call stack, so the
    // signal is already recorded — wait() would wait for a *second* one.
    QCOMPARE(disconnected.count(), 1);
    QCOMPARE(disconnected.at(0).at(0).toInt(), 1009);
}

void TestWebSocketClient::wssIsRefusedWithoutTls()
{
    WsTestServer server;
    server.setAbortOnConnect(true);
    QVERIFY(server.listen());

    Daqster::WebSocketClient client;
    client.setHandshakeTimeoutMs(2000);
    QSignalSpy connected(&client, &Daqster::WebSocketClient::connected);
    QSignalSpy errors(&client, &Daqster::WebSocketClient::errorOccurred);
    client.open(wsUrl(server, QStringLiteral("wss")));
    QVERIFY(errors.wait(5000));
    QCOMPARE(connected.count(), 0);
    QVERIFY(!client.isOpen());
}
