# WebSocket client (`WebSocketClient`) — as-built

[Български](./websocket-client.md) | [English](./websocket-client.en.md)

Parent: [Architecture](./README.en.md) · REQ: `REQ-SW-PL-087`

## Why not Qt WebSockets

`Qt::WebSockets` is a **separate Qt module** — not part of `QtBase`, missing on
many installs, and it would add a whole module to the framework's dependencies
for **one** client. The repo already ships a hand-written WebSocket *server*
in the control plane (`ControlPlaneServer`, RFC 6455 frames over `QTcpSocket`),
so the frame format is already here; only the client half was missing.
`WebSocketClient` is therefore `Qt::Network` **only** (`QTcpSocket` /
`QSslSocket`), lives in `DaqsterCommonLibrary`, namespace `Daqster`, and is the
one owner of the protocol instead of a third copy inside a plugin.

## Lifecycle

```cpp
Daqster::WebSocketClient client;
connect(&client, &Daqster::WebSocketClient::textMessage, ...);
client.open(QUrl("ws://127.0.0.1:8080/ws"));
// connected() → sendText()/sendBinary() → close(1000) → disconnected(code, reason)
```

`open()` is asynchronous: `connectToHost()` (or `connectToHostEncrypted()` for
`wss`), then the HTTP Upgrade. There is **no `waitFor*()` anywhere** — the class
is driven purely by socket signals, so it is safe on the GUI thread. `connected()`
is emitted only after a validated `Sec-WebSocket-Accept`; every failure is
`errorOccurred()`, and a failure after the handshake also emits `disconnected()`.

## Frame rules (RFC 6455)

- **Masking:** every client frame carries the mask bit and a random 4-byte key
  (§5.3) — a server MUST close the connection on an unmasked client frame.
- **Fragmentation:** `text`/`binary` with `FIN=0` accumulate; `continuation`
  (`0x0`) continues; the final `FIN=1` delivers the message **once, whole**. A
  new data frame mid-message and a continuation without a start are 1002.
- **Control frames** (`0x8`–`0xA`) are unfragmented and ≤ 125 bytes; `ping` is
  answered with `pong` carrying the same payload; `pong` is ignored.
- **Close handshake:** `close(code)` sends the close frame and waits for the
  peer's; `disconnected` reports the peer's code. No answer within 1.5 s aborts
  the socket (1006). Valid codes: `1000` and `3000…4999`; anything else becomes
  `1000` and is reported.
- **Protocol errors:** non-zero RSV bits or a **masked** server frame fail the
  connection with 1002.
- **Limits:** `maxMessageSize` (8 MiB default) is enforced on the header length
  and after reassembly — a larger message yields `errorOccurred` +
  `disconnected(1009, …)`. `handshakeTimeoutMs` (10 s default) bounds the
  Upgrade so "connecting" cannot hang forever.

## TLS (`wss://`)

`wss://` goes through `QSslSocket` (a `QTcpSocket` subclass), but only when
`QSslSocket::supportsSsl()` is true; otherwise `open()` refuses with a clear
message and never silently falls back to plain text. The Upgrade request is
written on `encrypted()`, not on `connected()`.

## What it does not do

- It is not a server and accepts no connections.
- No subprotocol negotiation (`Sec-WebSocket-Protocol`) and no `Authorization`
  header; no caller needs them today.
- No UTF-8 validation of text messages (delivered via `QString::fromUtf8`).

## Tests

`tests/plugins/common/test_websocket_client.{h,cpp}` in `daqster_common_tests`
(12 cases) against a tiny `QTcpServer` WebSocket server inside the test:
accept validation, masked client frame, text/binary round-trip, fragmentation,
ping→pong, close code, invalid accept, oversized message, `wss` refused against
a plain server.
