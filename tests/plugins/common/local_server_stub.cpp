// A minimal local HTTP server for `TestLocalServerService` (REQ-SW-PL-081).
//
// Why a separate executable and not a shell script: `LocalServerService` spawns
// a **process** and waits for readiness over `GET /health`, so the stub has to
// really listen on a port. The tests must not need a real server (gigabytes
// of weights, GPU, minutes to load) — this file is
// ~110 lines of Qt Core without a single model line and answers
// `200 {"status":"ok"}` on the given path.
//
// The arguments are the ones the class hands to every server (by default
// `--host`, `--port`), plus three keys used by the tests only:
//
//   --path <path>           which path answers 200 (default /health)
//   --exit-after-ms <N>     exits by itself after N ms ("the process died while running")
//   --exit-after-health     exits right after the first 200
//
// A server may also be started with other host/port flags (`--listen-ip`,
// `--listen-port` — `LocalServerSpec::portFlags`); both spellings are accepted.
// **Any other argument is an error** (exit 2): the test stub is as strict as a
// real server such as `sd-server`, so a wrong flag pair cannot pass unnoticed.
//
// Exit code 0 on a normal timer exit, 2 on bad arguments, 3 on a taken port.

#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <cstdio>

namespace {

QByteArray response(int status, const QByteArray& body)
{
    return QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status)
           + QByteArrayLiteral(" Stub\r\nContent-Type: application/json\r\n")
           + QByteArrayLiteral("Content-Length: ") + QByteArray::number(body.size())
           + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QStringList args = app.arguments();
    int port = 0;
    int exitAfterMs = 0;
    bool exitAfterHealth = false;
    QString path = QStringLiteral("/health");

    for (int i = 1; i < args.size(); ++i) {
        const QString& arg = args.at(i);
        const bool hasValue = (i + 1 < args.size());
        if ((arg == QLatin1String("--port") || arg == QLatin1String("--listen-port"))
            && hasValue) {
            port = args.at(++i).toInt();
        } else if ((arg == QLatin1String("--host")
                    || arg == QLatin1String("--listen-ip")) && hasValue) {
            ++i;   // we listen on localhost only — the value is for real servers
        } else if (arg == QLatin1String("--path") && hasValue) {
            path = args.at(++i);
        } else if (arg == QLatin1String("--exit-after-ms") && hasValue) {
            exitAfterMs = args.at(++i).toInt();
        } else if (arg == QLatin1String("--exit-after-health")) {
            exitAfterHealth = true;
        } else {
            std::fprintf(stderr, "local_server_stub: unknown argument \"%s\"\n",
                         qPrintable(arg));
            return 2;
        }
    }

    if (port <= 0) {
        std::fprintf(stderr, "local_server_stub: missing --port\n");
        return 2;
    }

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, quint16(port))) {
        std::fprintf(stderr, "local_server_stub: port %d is taken\n", port);
        return 3;
    }
    std::printf("local_server_stub: listening on 127.0.0.1:%d (%s)\n",
                port, qPrintable(path));
    std::fflush(stdout);

    // Every connection serves itself: wait for the request line, answer, close.
    // Enough for `GET /health` (and for the class' probe, which is the same
    // request).
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        while (QTcpSocket* socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                // The request may arrive in pieces: wait for the end of the
                // first line.
                QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
                if (!buffer.contains('\n')) {
                    socket->setProperty("buffer", buffer);
                    return;
                }
                const QList<QByteArray> line = buffer.split('\n').value(0).trimmed().split(' ');
                const QString requested = QString::fromLatin1(line.value(1));

                if (line.value(0) == "GET" && requested == path) {
                    socket->write(response(200, QByteArrayLiteral("{\"status\":\"ok\"}")));
                    socket->disconnectFromHost();
                    if (exitAfterHealth) {
                        socket->flush();
                        QTimer::singleShot(0, &app, [] { QCoreApplication::exit(0); });
                    }
                } else {
                    socket->write(response(404, QByteArrayLiteral("{\"error\":\"not found\"}")));
                    socket->disconnectFromHost();
                }
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket,
                             &QTcpSocket::deleteLater);
        }
    });

    if (exitAfterMs > 0)
        QTimer::singleShot(exitAfterMs, &app, [] { QCoreApplication::exit(0); });

    return app.exec();
}
