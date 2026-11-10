# WebSocket клиент (`WebSocketClient`) — as-built

[Български](./websocket-client.md) | [English](./websocket-client.en.md)

Родител: [Architecture](./README.md) · REQ: `REQ-SW-PL-087`

## Защо не Qt WebSockets

`Qt::WebSockets` е **отделен Qt модул**: не е част от `QtBase`, липсва при
много инсталации (вкл. част от CI образите) и би добавил цял модул към
зависимостите на framework-а заради **един** клиент. Проектът вече има
ръкописен WebSocket **сървър** в контролната плоскост
(`ControlPlaneServer`, RFC 6455 кадри на `QTcpSocket`) — тоест форматът на
кадрите е вече в репото, а липсваше само клиентската половина. Затова
`WebSocketClient` е `Qt::Network` **само** (`QTcpSocket` / `QSslSocket`), в
`DaqsterCommonLibrary`, namespace `Daqster` — един собственик на протокола, а
не трето копие в плъгин.

## Жизнен цикъл

```cpp
Daqster::WebSocketClient client;
connect(&client, &Daqster::WebSocketClient::textMessage, ...);
client.open(QUrl("ws://127.0.0.1:8080/ws"));
// connected() → sendText()/sendBinary() → close(1000) → disconnected(code, reason)
```

`open()` е асинхронен: `connectToHost()` (или `connectToHostEncrypted()` за
`wss`), после HTTP Upgrade. **Няма нито един `waitFor*()`** — всичко е по
сигнали на сокета, тоест класът е безопасен за GUI нишката.

| Сигнал | Кога |
|---|---|
| `connected()` | едва след **валидиран** `Sec-WebSocket-Accept` |
| `textMessage(QString)` | цяло (евентуално сглобено) текстово съобщение |
| `binaryMessage(QByteArray)` | цяло бинарно съобщение |
| `disconnected(int code, QString reason)` | close handshake или 1006 при скъсана връзка |
| `errorOccurred(QString)` | лош upgrade, протоколна грешка, мрежова грешка |

## Правила на кадрите (RFC 6455)

- **Маскиране:** всеки клиентски кадър носи mask bit и случаен 4-байтов ключ
  (§5.3). Сървър, който получи немаркиран клиентски кадър, е длъжен да затвори
  връзката — затова маскирането не е „подробност“, а договор.
- **Fragmentация:** `text`/`binary` с `FIN=0` се трупат в буфер, следващите
  `continuation` (`0x0`) го продължават, последният `FIN=1` го доставя **веднъж,
  цяло**. Ново съобщение преди края на предишното и continuation без начало са
  протоколна грешка (1002).
- **Control кадри** (`0x8`–`0xA`) не могат да бъдат фрагментирани и са ≤ 125
  байта. `ping` → веднага `pong` със **същия** payload; `pong` се игнорира.
- **Close handshake:** `close(code)` праща close frame (код + причина) и чака
  ответния; `disconnected` носи кода на сървъра. Ако сървърът не отговори в
  `kCloseGraceMs` (1.5 s), сокетът се затваря принудително (1006). Валидни
  кодове: `1000` и `3000…4999`; невалиден се заменя с `1000` и се съобщава.
- **RSV битове** трябва да са 0 (няма разширения) и frame от сървъра **не**
  може да е маскиран — и двете са 1002.
- **Лимити:** `maxMessageSize` (по подразбиране **8 MiB**) се проверява и по
  дължината в хедъра, и след сглобяване; по-голямо съобщение → `errorOccurred`
  + `disconnected(1009, …)`. `handshakeTimeoutMs` (по подразбиране 10 s) слага
  граница на Upgrade-а, за да не виси „свързвам се“ завинаги.

## TLS (`wss://`)

`wss://` минава през `QSslSocket` (наследник на `QTcpSocket`), но само ако
`QSslSocket::supportsSsl()` е истина; иначе `open()` отказва с ясно съобщение
(„изисква TLS, а тази Qt инсталация няма SSL backend“) и **не** пада тихо към
plain текст. Upgrade заявката се пише едва на `encrypted()`, не на `connected()`.

## Какво НЕ прави

- Не е сървър и не приема връзки.
- Не договаря подпротоколи (`Sec-WebSocket-Protocol`) и не праща
  `Authorization` — потребителят, който има нужда, слага хедъри в бъдещ
  overload; днес няма такъв потребител.
- Не валидира UTF-8 на текстовите съобщения (доставя `QString::fromUtf8`).

## Тестове

`tests/plugins/common/test_websocket_client.{h,cpp}` в `daqster_common_tests`
(12 случая) с мини WebSocket сървър `QTcpServer` в самия тест: валидация на
accept, маскиран клиентски кадър, text/binary round-trip, фрагментация,
ping→pong, close код, невалиден accept, твърде голямо съобщение, `wss`
отказан срещу plain сървър.
