// SPDX-License-Identifier: MIT
//
// WebSocketClient — RFC 6455 client over QTcpSocket/QSslSocket (REQ-SW-PL-087).

#include "WebSocketClient.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kCloseGraceMs    = 1500;    // wait for the peer's close frame
constexpr quint8 kOpContinuation = 0x0;
constexpr quint8 kOpText          = 0x1;
constexpr quint8 kOpBinary        = 0x2;
constexpr quint8 kOpClose         = 0x8;
constexpr quint8 kOpPing          = 0x9;
constexpr quint8 kOpPong          = 0xA;

/// RFC 6455 §1.3: SHA-1 of the client key plus the protocol GUID.
QByteArray acceptForKey(const QByteArray& key)
{
    static const QByteArray guid = QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    return QCryptographicHash::hash(key + guid, QCryptographicHash::Sha1).toBase64();
}

bool isValidCloseCode(int code)
{
    return code == 1000 || (code >= 3000 && code <= 4999);
}

} // namespace

namespace Daqster {

WebSocketClient::WebSocketClient(QObject* parent)
    : QObject(parent)
{
    m_handshakeTimer = new QTimer(this);
    m_handshakeTimer->setSingleShot(true);
    connect(m_handshakeTimer, &QTimer::timeout, this, [this] {
        fail(1002, tr("WebSocket: сървърът не отговори на Upgrade заявката навреме"));
    });

    m_closeTimer = new QTimer(this);
    m_closeTimer->setSingleShot(true);
    connect(m_closeTimer, &QTimer::timeout, this, [this] {
        if (m_socket)
            m_socket->abort();
        tearDown(1006, tr("WebSocket: затварянето не беше потвърдено от сървъра"), true);
    });
}

WebSocketClient::~WebSocketClient()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        delete m_socket;
        m_socket = nullptr;
    }
}

void WebSocketClient::open(const QUrl& url)
{
    resetSocket();
    m_url = url;

    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("ws") && scheme != QLatin1String("wss")) {
        fail(1002, tr("WebSocket: неподдържана схема „%1“ — очаква се ws:// или wss://")
                      .arg(url.scheme()));
        return;
    }
    if (url.host().isEmpty()) {
        fail(1002, tr("WebSocket: липсва хост в адреса „%1“").arg(url.toString()));
        return;
    }

    m_secure = scheme == QLatin1String("wss");
    if (m_secure && !QSslSocket::supportsSsl()) {
        fail(1002, tr("WebSocket: wss:// изисква TLS, а тази Qt инсталация няма SSL backend"));
        return;
    }

    const quint16 port = static_cast<quint16>(
        url.port(m_secure ? 443 : 80));

    if (m_secure) {
        auto* ssl = new QSslSocket(this);
        m_socket = ssl;
        connect(ssl, &QSslSocket::encrypted, this, [this] { sendHandshake(); });
    } else {
        m_socket = new QTcpSocket(this);
        connect(m_socket, &QTcpSocket::connected, this, [this] { sendHandshake(); });
    }

    connect(m_socket, &QAbstractSocket::readyRead, this, [this] {
        if (m_handshakeDone)
            processIncoming();
        else
            handleHandshakeData();
    });
    connect(m_socket, &QAbstractSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                const QString reason = m_socket ? m_socket->errorString()
                                                : tr("неизвестна грешка");
                fail(1006, tr("WebSocket: мрежова грешка — %1").arg(reason));
            });
    connect(m_socket, &QAbstractSocket::disconnected, this, [this] {
        if (!m_closeReceived && m_handshakeDone)
            tearDown(1006, tr("WebSocket: връзката прекъсна без close frame"), true);
    });

    m_handshakeTimer->start(m_handshakeTimeoutMs);
    if (m_secure)
        static_cast<QSslSocket*>(m_socket)->connectToHostEncrypted(url.host(), port);
    else
        m_socket->connectToHost(url.host(), port);
}

void WebSocketClient::sendHandshake()
{
    quint32 keyWords[4] = {0, 0, 0, 0};
    QRandomGenerator::system()->fillRange(keyWords);
    const QByteArray key =
        QByteArray(reinterpret_cast<const char*>(keyWords), sizeof(keyWords)).toBase64();
    m_expectedAccept = acceptForKey(key);

    const quint16 port = static_cast<quint16>(m_url.port(m_secure ? 443 : 80));
    const QString hostHeader = port == (m_secure ? 443 : 80)
                                   ? m_url.host()
                                   : QStringLiteral("%1:%2").arg(m_url.host()).arg(port);
    QString target = m_url.path(QUrl::FullyEncoded);
    if (target.isEmpty())
        target = QStringLiteral("/");
    if (m_url.hasQuery())
        target += QLatin1Char('?') + m_url.query(QUrl::FullyEncoded);

    QByteArray request;
    request += "GET " + target.toUtf8() + " HTTP/1.1\r\n";
    request += "Host: " + hostHeader.toUtf8() + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + key + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "\r\n";

    if (m_socket)
        m_socket->write(request);
}

void WebSocketClient::handleHandshakeData()
{
    m_readBuffer += m_socket ? m_socket->readAll() : QByteArray();
    const int headerEnd = m_readBuffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    const QByteArray header = m_readBuffer.left(headerEnd);
    m_readBuffer.remove(0, headerEnd + 4);

    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty() || !lines.first().trimmed().startsWith("HTTP/1.1 101")) {
        fail(1002, tr("WebSocket: Upgrade беше отказан (%1)")
                      .arg(QString::fromLatin1(lines.isEmpty() ? QByteArray() : lines.first().trimmed())));
        return;
    }

    QByteArray accept;
    bool upgradeSeen = false;
    bool connectionSeen = false;
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QByteArray name = line.left(colon).trimmed().toLower();
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (name == "sec-websocket-accept")
            accept = value;
        else if (name == "upgrade" && value.toLower().contains("websocket"))
            upgradeSeen = true;
        else if (name == "connection" && value.toLower().contains("upgrade"))
            connectionSeen = true;
    }

    if (!upgradeSeen || !connectionSeen) {
        fail(1002, tr("WebSocket: отговорът не съдържа Upgrade: websocket / Connection: Upgrade"));
        return;
    }
    if (accept != m_expectedAccept) {
        fail(1002, tr("WebSocket: невалиден Sec-WebSocket-Accept — възможно е подправен сървър"));
        return;
    }

    m_handshakeDone = true;
    m_handshakeTimer->stop();
    emit connected();
    if (!m_readBuffer.isEmpty())
        processIncoming();
}

bool WebSocketClient::handleFrame(quint8 opcode, bool fin, const QByteArray& payload)
{
    const bool control = opcode >= kOpClose;
    if (control && (!fin || payload.size() > 125)) {
        fail(1002, tr("WebSocket: невалиден control frame"));
        return false;
    }

    switch (opcode) {
    case kOpPing:
        sendFrame(kOpPong, payload);
        return true;
    case kOpPong:
        return true;
    case kOpClose: {
        m_closeReceived = true;
        int code = 1005;
        QString reason;
        if (payload.size() >= 2) {
            code = (static_cast<quint8>(payload.at(0)) << 8)
                   | static_cast<quint8>(payload.at(1));
            reason = QString::fromUtf8(payload.mid(2));
        }
        if (!m_closeSent)
            sendFrame(kOpClose, payload.left(2));
        tearDown(code, reason, true);
        return false;
    }
    default:
        break;
    }

    if (opcode == kOpContinuation) {
        if (m_fragmentOpcode == 0) {
            fail(1002, tr("WebSocket: continuation frame без начало на съобщение"));
            return false;
        }
    } else if (opcode == kOpText || opcode == kOpBinary) {
        if (m_fragmentOpcode != 0) {
            fail(1002, tr("WebSocket: ново съобщение преди края на предишното"));
            return false;
        }
        m_fragmentOpcode = opcode;
    } else {
        fail(1002, tr("WebSocket: непознат opcode 0x%1").arg(opcode, 2, 16, QLatin1Char('0')));
        return false;
    }

    m_fragmentBuffer += payload;
    if (m_fragmentBuffer.size() > m_maxMessageSize) {
        fail(1009, tr("WebSocket: съобщение по-голямо от %1 байта")
                       .arg(m_maxMessageSize));
        return false;
    }
    if (!fin)
        return true;

    const quint8 messageOpcode = m_fragmentOpcode;
    const QByteArray message = m_fragmentBuffer;
    m_fragmentBuffer.clear();
    m_fragmentOpcode = 0;

    if (messageOpcode == kOpText)
        emit textMessage(QString::fromUtf8(message));
    else
        emit binaryMessage(message);
    return true;
}

void WebSocketClient::processIncoming()
{
    if (!m_socket)
        return;
    m_readBuffer += m_socket->readAll();

    while (m_readBuffer.size() >= 2) {
        const quint8 b0 = static_cast<quint8>(m_readBuffer.at(0));
        const quint8 b1 = static_cast<quint8>(m_readBuffer.at(1));
        const bool fin = (b0 & 0x80) != 0;
        const quint8 opcode = b0 & 0x0F;

        if ((b0 & 0x70) != 0) {
            fail(1002, tr("WebSocket: RSV битове без разширение"));
            return;
        }
        if ((b1 & 0x80) != 0) {
            fail(1002, tr("WebSocket: frame от сървъра е маскиран (забранено от RFC 6455)"));
            return;
        }

        const quint8 length7 = b1 & 0x7F;
        int headerLength = 2;
        quint64 payloadLength = length7;
        if (length7 == 126) {
            if (m_readBuffer.size() < 4)
                return;
            payloadLength = (static_cast<quint8>(m_readBuffer.at(2)) << 8)
                            | static_cast<quint8>(m_readBuffer.at(3));
            headerLength = 4;
        } else if (length7 == 127) {
            if (m_readBuffer.size() < 10)
                return;
            payloadLength = 0;
            for (int i = 0; i < 8; ++i)
                payloadLength = (payloadLength << 8)
                                | static_cast<quint8>(m_readBuffer.at(2 + i));
            headerLength = 10;
        }
        if (payloadLength > static_cast<quint64>(m_maxMessageSize)) {
            fail(1009, tr("WebSocket: съобщение по-голямо от %1 байта")
                           .arg(m_maxMessageSize));
            return;
        }
        if (static_cast<quint64>(m_readBuffer.size()) < headerLength + payloadLength)
            return;

        const QByteArray payload = m_readBuffer.mid(headerLength,
                                                    static_cast<int>(payloadLength));
        m_readBuffer.remove(0, headerLength + static_cast<int>(payloadLength));

        if (!handleFrame(opcode, fin, payload))
            return;
    }
}

void WebSocketClient::sendFrame(quint8 opcode, const QByteArray& payload)
{
    if (!m_socket || !m_handshakeDone || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(tr("WebSocket: сокетът не е отворен — кадърът не е изпратен"));
        return;
    }

    QByteArray frame;
    frame.append(static_cast<char>(0x80 | (opcode & 0x0F)));

    const quint64 length = static_cast<quint64>(payload.size());
    if (length < 126) {
        frame.append(static_cast<char>(0x80 | length));
    } else if (length <= 0xFFFF) {
        frame.append(static_cast<char>(0x80 | 126));
        frame.append(static_cast<char>((length >> 8) & 0xFF));
        frame.append(static_cast<char>(length & 0xFF));
    } else {
        frame.append(static_cast<char>(0x80 | 127));
        for (int i = 7; i >= 0; --i)
            frame.append(static_cast<char>((length >> (8 * i)) & 0xFF));
    }

    // RFC 6455 §5.3: a client MUST mask every frame.
    const quint32 mask = QRandomGenerator::system()->generate();
    frame.append(static_cast<char>((mask >> 24) & 0xFF));
    frame.append(static_cast<char>((mask >> 16) & 0xFF));
    frame.append(static_cast<char>((mask >> 8) & 0xFF));
    frame.append(static_cast<char>(mask & 0xFF));
    for (int i = 0; i < payload.size(); ++i) {
        const char key = static_cast<char>((mask >> (8 * (3 - (i % 4)))) & 0xFF);
        frame.append(static_cast<char>(payload.at(i) ^ key));
    }

    m_socket->write(frame);
}

void WebSocketClient::sendText(const QString& text)
{
    sendFrame(kOpText, text.toUtf8());
}

void WebSocketClient::sendBinary(const QByteArray& data)
{
    sendFrame(kOpBinary, data);
}

void WebSocketClient::close(int code)
{
    if (!m_socket)
        return;
    if (!isValidCloseCode(code)) {
        emit errorOccurred(tr("WebSocket: невалиден close код %1 — използва се 1000").arg(code));
        code = 1000;
    }
    if (!m_handshakeDone) {
        resetSocket();
        return;
    }
    if (m_closeSent)
        return;

    QByteArray payload;
    payload.append(static_cast<char>((code >> 8) & 0xFF));
    payload.append(static_cast<char>(code & 0xFF));
    sendFrame(kOpClose, payload);
    m_closeSent = true;
    m_closeTimer->start(kCloseGraceMs);
}

bool WebSocketClient::isOpen() const
{
    return m_handshakeDone && m_socket
           && m_socket->state() == QAbstractSocket::ConnectedState && !m_closeSent;
}

void WebSocketClient::setMaxMessageSize(int bytes)
{
    m_maxMessageSize = bytes > 0 ? bytes : 8 * 1024 * 1024;
}

int WebSocketClient::maxMessageSize() const
{
    return m_maxMessageSize;
}

void WebSocketClient::setHandshakeTimeoutMs(int ms)
{
    m_handshakeTimeoutMs = ms > 0 ? ms : 10000;
}

int WebSocketClient::handshakeTimeoutMs() const
{
    return m_handshakeTimeoutMs;
}

QUrl WebSocketClient::url() const
{
    return m_url;
}

void WebSocketClient::tearDown(int code, const QString& reason, bool emitDisconnected)
{
    m_handshakeTimer->stop();
    m_closeTimer->stop();
    resetSocket();
    if (emitDisconnected && !m_disconnectedEmitted) {
        m_disconnectedEmitted = true;
        emit disconnected(code, reason);
    }
}

void WebSocketClient::fail(int code, const QString& error)
{
    const bool wasConnected = m_handshakeDone;
    emit errorOccurred(error);
    tearDown(code, error, wasConnected);
}

void WebSocketClient::resetSocket()
{
    m_handshakeTimer->stop();
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_readBuffer.clear();
    m_fragmentBuffer.clear();
    m_expectedAccept.clear();
    m_fragmentOpcode = 0;
    m_handshakeDone = false;
    m_closeSent = false;
    m_closeReceived = false;
    m_disconnectedEmitted = false;
}

} // namespace Daqster
