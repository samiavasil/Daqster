#pragma once

#include <QtTest>

/// Tests for `Daqster::WebSocketClient` (REQ-SW-PL-087) — the RFC 6455 client
/// half of the framework WebSocket support.
///
/// The peer is a tiny in-test server (`QTcpServer` + ~150 lines below the test
/// class): it performs the upgrade, validates that every client frame is
/// masked, and echoes/forges exactly the frame the case needs. No external
/// server, no port assumption, no Qt WebSockets.
///
/// Guarded contract:
///  1. the upgrade is only accepted when `Sec-WebSocket-Accept` matches;
///  2. every client frame carries the mask bit (RFC 6455 §5.3);
///  3. text and binary round-trip losslessly;
///  4. a fragmented message (continuation frames) is delivered once, whole;
///  5. a ping is answered with a pong carrying the same payload;
///  6. `close(code)` performs the close handshake and reports the code;
///  7. an oversized message fails the connection with 1009;
///  8. `wss://` without TLS is refused instead of silently going plain.
class TestWebSocketClient : public QObject
{
    Q_OBJECT

private slots:
    /// 101 + correct accept → `connected()`, `isOpen()`.
    void handshakeValidatesAccept();
    /// A client frame on the wire has the mask bit set.
    void clientFramesAreMasked();
    /// Text (UTF-8) reaches the peer and comes back as one `textMessage`.
    void textRoundTrip();
    /// Binary payload survives byte for byte.
    void binaryRoundTrip();
    /// Three frames (text + continuation + continuation) → one message.
    void fragmentedMessageIsReassembled();
    /// `ping` from the peer → `pong` with the same payload.
    void pingIsAnsweredWithPong();
    /// `close(code)` → the peer sees the code and the client reports it.
    void closeCodeRoundTrip();
    /// A wrong `Sec-WebSocket-Accept` is rejected, `connected()` never fires.
    void invalidAcceptIsRejected();
    /// A message above `maxMessageSize` fails with 1009.
    void oversizedMessageIsRejected();
    /// `wss://` against a plain TCP peer must fail, never connect in the clear.
    void wssIsRefusedWithoutTls();
};
