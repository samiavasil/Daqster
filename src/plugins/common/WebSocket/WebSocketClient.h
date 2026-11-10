#pragma once

// SPDX-License-Identifier: MIT
//
// WebSocketClient — a framework WebSocket **client** (REQ-SW-PL-087).
//
// The repo had a hand-written WebSocket *server* inside the control plane and
// no client at all: every consumer that needed to talk to a local WS server
// (or to the control plane itself) had to bring its own implementation, and
// the obvious shortcut — Qt WebSockets — is deliberately **not** a dependency
// of this project (it is a separate Qt module, absent on many installs, and it
// would size the dependency for one client). So the client is Qt::Network
// only: QTcpSocket + RFC 6455, the same frame rules the server half already
// speaks.
//
// What the class is responsible for:
//
//   * the HTTP Upgrade handshake (`Sec-WebSocket-Key` → validate
//     `Sec-WebSocket-Accept`), not trusting a 101 alone;
//   * a streaming frame parser with fragmentation (continuation frames),
//     ping→pong, and a close handshake with a code and a reason;
//   * **masked** client frames — RFC 6455 §5.3, a server MUST close a
//     connection that receives an unmasked client frame, so this is not a
//     detail;
//   * a hard cap on a reassembled message (`maxMessageSize`, 8 MiB default) so
//     a broken/ hostile peer cannot grow the buffer without bound.
//
// What it is **not**: no `waitFor*()` anywhere. Everything is driven by
// QTcpSocket signals on the caller's thread, so the class is usable from the
// GUI thread without blocking it. `wss://` goes through `QSslSocket` when Qt
// was built with TLS; without TLS the URL is refused with a clear error
// instead of silently connecting in plain text.

#include "DaqsterCommonExport.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

class QTcpSocket;
class QTimer;

namespace Daqster {

/**
 * @brief Client side of RFC 6455 over a plain (or TLS) TCP socket.
 *
 * Lifecycle: `open(url)` → `connected()` → `sendText()`/`sendBinary()` →
 * `close(code)` → `disconnected(code, reason)`. Every failure is reported
 * through `errorOccurred()`; a failure after the handshake also emits
 * `disconnected()` with the WebSocket close code the client decided on (1002
 * protocol error, 1009 message too big, 1006 abnormal closure).
 *
 * Incoming frames are delivered on the thread that owns the object (the socket
 * is created with this object as its parent), so a consumer can connect the
 * signals to its own slots without extra synchronisation.
 */
class DAQSTERCOMMON_EXPORT WebSocketClient : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketClient(QObject* parent = nullptr);
    ~WebSocketClient() override;

    /**
     * @brief Starts the connection and the HTTP Upgrade handshake.
     *
     * Only `ws` and `wss` are accepted. An invalid scheme, an empty host, or a
     * `wss` URL in a Qt build without TLS is reported through
     * `errorOccurred()` and no socket is opened.
     *
     * @param url Absolute URL; its path and query become the upgrade request
     *            target, the port defaults to 80/443.
     */
    void open(const QUrl& url);

    /**
     * @brief Starts the close handshake and reports `disconnected()` once the
     *        peer answers (or after a bounded wait if it does not).
     *
     * @param code Close code; must be 1000 or in 3000…4999.
     */
    void close(int code = 1000);

    /// True between a validated handshake and the close handshake.
    bool isOpen() const;

    /// Sends one masked text frame (opcode 0x1). No-op (with an error) when closed.
    void sendText(const QString& text);

    /// Sends one masked binary frame (opcode 0x2). No-op (with an error) when closed.
    void sendBinary(const QByteArray& data);

    /// Reassembled-message cap; larger messages fail with close code 1009.
    void setMaxMessageSize(int bytes);
    int maxMessageSize() const;

    /// How long the HTTP Upgrade may take before the socket is dropped.
    void setHandshakeTimeoutMs(int ms);
    int handshakeTimeoutMs() const;

    /// The URL passed to the last `open()`.
    QUrl url() const;

signals:
    /// The handshake was validated (`Sec-WebSocket-Accept` matches).
    void connected();

    /// The connection is gone; `code` is the WebSocket close code (1006 when
    /// the socket died without a close frame).
    void disconnected(int code, const QString& reason);

    /// A complete (possibly reassembled) text message.
    void textMessage(const QString& text);

    /// A complete (possibly reassembled) binary message.
    void binaryMessage(const QByteArray& data);

    /// A human-readable failure: bad upgrade, protocol violation, socket error.
    void errorOccurred(const QString& error);

private:
    /// Writes the HTTP Upgrade request (after TLS, if any).
    void sendHandshake();

    /// Reads the upgrade response and validates `Sec-WebSocket-Accept`.
    void handleHandshakeData();

    /// Handles one data/control frame; returns false on a protocol error.
    bool handleFrame(quint8 opcode, bool fin, const QByteArray& payload);

    /// Sends one frame with FIN set; masks the payload (client rule).
    void sendFrame(quint8 opcode, const QByteArray& payload);

    /// Consumes as many complete frames as the read buffer holds.
    void processIncoming();

    /// Drops the socket and state; `emitDisconnected` decides the signal.
    void tearDown(int code, const QString& reason, bool emitDisconnected);

    /// Reports the error and tears the connection down with `code`.
    void fail(int code, const QString& error);

    /// Stops the handshake timer and the socket, keeping the object reusable.
    void resetSocket();

    QTcpSocket*   m_socket = nullptr;
    QByteArray    m_readBuffer;
    QByteArray    m_fragmentBuffer;
    QByteArray    m_expectedAccept;
    quint8        m_fragmentOpcode = 0;
    int           m_maxMessageSize = 8 * 1024 * 1024;
    int           m_handshakeTimeoutMs = 10000;
    QUrl          m_url;
    QTimer*       m_handshakeTimer = nullptr;
    QTimer*       m_closeTimer = nullptr;
    bool          m_handshakeDone = false;
    bool          m_closeSent = false;
    bool          m_closeReceived = false;
    bool          m_disconnectedEmitted = false;
    bool          m_secure = false;
};

} // namespace Daqster
