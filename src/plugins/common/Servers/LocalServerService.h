#pragma once

// SPDX-License-Identifier: MIT
//
// LocalServerService — one supervisor for any local HTTP server (REQ-SW-PL-081).
//
// The life of a local HTTP server is the same whoever the server is:
// free port → launch → `GET /health` until ready → ownership file (so a second
// scene can reuse the process) → bounded stop. Written twice before this class
// existed (two client plugins, each supervising its own server process), and a
// third copy was about to be written — so it is one class instead.
//
// The class knows **nothing** about models, inference, VRAM or nodes: it is
// given a `LocalServerSpec` (what to run) and reports when the server is ready
// and on which port. The protocol *after* readiness is not its business — that
// belongs to the client of the particular service.
//
// **Ownership (two states that never mix):**
//
//   * `own-runner` — we spawned the process (`isOwned() == true`): `stop()`
//     shuts down only that one, in bounded time (`terminate → grace → kill →
//     killWait`);
//   * `external` — someone else already answers on the well-known port and we
//     **reuse** it (`isReused() == true`, `isOwned() == false`): `stop()`
//     closes the connection and leaves the foreign process alone — it has its
//     own owner. Its ownership record stays only while that process lives: a
//     dead pid is garbage and `stop()` drops it.
//
// **One service, two processes (the start lock):** the ownership file says who
// runs a `kind`, and "read it, find nothing, launch" is only safe when nobody
// else does the same at the same time — otherwise two starts both read an empty
// file and both launch a server. `start()` takes an **interprocess lock**
// (`<GenericDataLocation>/Daqster/local-servers.lock`) around that decision,
// re-reads the record inside it (a live pid means a starting/running server →
// reuse) and releases it as soon as the record exists. The `/health` wait is
// always outside the lock: it may take minutes, the decision takes milliseconds.
// `stop()` and `sweepOrphans()` take the same lock briefly, because they write
// the same file.
//
// **No blocking of the GUI thread:** not a single `waitForStarted()` /
// `waitForFinished()` on the start path; readiness is `QTimer` +
// `QNetworkAccessManager`, never a blocking wait. The only bounded waits are in
// `stop()`/`fail()` — there `Daqster::ProcessOwner` requires them (REQ-SW-PL-050).

#include "DaqsterCommonExport.h"

#include <QElapsedTimer>
#include <QLockFile>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "Threading/ProcessOwner.h"

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

namespace Daqster {

/**
 * @brief What to run and how to wait for readiness.
 *
 * Every field is about the life of the process: `kind` is the name the
 * ownership record is recognised by between starts (e.g. `sd-server`), and
 * `arguments` must **not** contain the host/port flags (`portFlags`) — the
 * class appends them so a caller cannot lose the port it will later ask
 * `/health` on. A spec that repeats them is rejected in `start()`.
 */
struct DAQSTERCOMMON_EXPORT LocalServerSpec
{
    QString     kind;        ///< `whisper-server` | `piper` | …
    QString     binary;      ///< absolute path to the executable
    QStringList arguments;   ///< arguments without the host/port flags
    /**
     * @brief The two flags this server takes for host and port, in that order.
     *
     * First entry = host flag, second = port flag. The default fits the
     * common `--host <h> --port <n>`; a server with different names sets its
     * own pair (e.g. `sd-server`: `--listen-ip`, `--listen-port`). The list
     * must be either empty (→ the defaults) or exactly two **different**
     * non-empty entries; anything else is rejected by `start()`.
     */
    QStringList portFlags{ QStringLiteral("--host"), QStringLiteral("--port") };
    QString     host = QStringLiteral("127.0.0.1");
    int         preferredPort = 0;      ///< 0 = the first free port in the range
    int         portRangeStart = 18080; ///< start of the auto-selection range
    int         portRangeEnd   = 18180; ///< the end is **not** included
    QString     healthPath = QStringLiteral("/health");
    int         readyTimeoutMs  = 120000;  ///< a model may load for minutes
    int         healthIntervalMs = 500;    ///< how often `/health` is asked
    int         stopGraceMs = 3000;        ///< grace after `terminate()`
    int         stopKillMs  = 1000;        ///< wait after `kill()`
};

/**
 * @brief Owns the life of **any** local HTTP server.
 *
 * Typical use:
 * @code
 *   Daqster::LocalServerService service;
 *   connect(&service, &LocalServerService::started, …);   // port
 *   connect(&service, &LocalServerService::failed,  …);   // reason
 *   service.start(spec);          // returns immediately
 *   …
 *   service.stop();               // idempotent, bounded
 * @endcode
 *
 * The instance belongs to the thread that created it (as `QProcess` and
 * `QNetworkAccessManager` do); `stop()` blocks that thread for at most
 * `stopGraceMs + stopKillMs`.
 */
class DAQSTERCOMMON_EXPORT LocalServerService : public QObject
{
    Q_OBJECT

public:
    /// State of the process (not of the configuration — that is the caller's).
    enum class State {
        Stopped,   ///< nothing runs (never started, stopped, or foreign)
        Starting,  ///< process spawned (or a foreign one answered 503) — waiting for `/health`
        Ready,     ///< `healthPath` answered `200`
        Failed,    ///< start/readiness failed (see `lastError()`)
    };

    explicit LocalServerService(QObject* parent = nullptr);
    ~LocalServerService() override;

    /**
     * @brief Starts the server asynchronously; no-op when already
     *        `Starting`/`Ready`.
     *
     * The order is: sweep orphaned processes of the same kind → take the
     * interprocess start lock and re-read the ownership record (**a live record
     * means a starting/running server → reuse**, a dead one is removed) → ask
     * `healthPath` on the well-known port and reuse whoever answers → spawn our
     * own process only when nobody answered.
     *
     * The lock is released as soon as the record for our process exists (or the
     * decision is "reuse"), so the readiness wait never holds it.
     *
     * @param spec   what to run (remembered for `stop()`/`spec()`)
     * @param error  when given and the spec is unusable (missing binary,
     *               invalid range), the reason lands here and `failed()` is
     *               emitted **synchronously** — no start has begun.
     */
    void start(const LocalServerSpec& spec, QString* error = nullptr);

    /**
     * @brief Stops the server: `terminate()` → grace → `kill()` → killWait.
     *
     * Shuts down **only** a process we spawned; a reused foreign one stays
     * alive. For a reused one the ownership record is kept only while its
     * process lives (a dead pid makes it garbage, so it is dropped). Idempotent:
     * the second and later calls do nothing. Before `started` (the process has
     * not exec'ed yet) `ProcessOwner` waits for the start and sends **no**
     * signal — the invariant from REQ-SW-PL-050.
     */
    void stop();

    State state() const { return m_state; }
    /// Ready (`Ready`); the signal is `started(port)`.
    bool isReady() const { return m_state == State::Ready; }

    /// The real port (0 until one is chosen).
    int port() const { return m_port; }
    /// Base address, e.g. `http://127.0.0.1:18080`.
    QUrl baseUrl() const;
    /// Did we spawn the process (so `stop()` will shut it down)?
    bool isOwned() const { return m_owned; }
    /// Is this an already running (foreign) server we adopted?
    bool isReused() const { return m_reused; }

    QString kind() const { return m_spec.kind; }
    const LocalServerSpec& spec() const { return m_spec; }
    /// The reason for `Failed` (empty when there is no failure).
    QString lastError() const { return m_lastError; }

    /// The first free port in `[firstPort, lastPort)` (0 when there is none).
    ///
    /// The port is probed with a real `listen()` and released immediately —
    /// there is a small window between the probe and the server's bind, so
    /// `launch()` retries on the next port when it hears "couldn't bind" (see
    /// `onProcessFinished`).
    static int pickFreePort(int firstPort, int lastPort,
                            const QString& host = QStringLiteral("127.0.0.1"));

    /// Path of the ownership file
    /// (`<GenericDataLocation>/Daqster/local-servers.json`).
    static QString ownershipFilePath();

    /// Path of the interprocess start lock
    /// (`<GenericDataLocation>/Daqster/local-servers.lock`) — the same
    /// directory as the ownership file, never a second one.
    static QString lockFilePath();

Q_SIGNALS:
    /// `healthPath` answered `200` — the server accepts requests.
    void started(int port);
    /// Start/readiness failed; the process (when ours) has been stopped.
    void failed(const QString& reason);
    /// One line of the process' stdout/stderr (for the consumer's log).
    void logLine(const QString& line);

private:
    /// The effective host flag (`portFlags[0]`, default `--host`).
    QString hostFlag() const;
    /// The effective port flag (`portFlags[1]`, default `--port`).
    QString portFlag() const;
    /// `portFlags` is empty or exactly two different non-empty entries; on a
    /// defect `reason` receives the message and `false` is returned.
    bool portFlagsAreValid(QString* reason) const;
    /// The banned flags (host + port) that already sit in `arguments`.
    QStringList repeatedPortFlags() const;
    /// Sweeps orphaned processes of the same kind inside our port range (logged).
    void sweepOrphans();
    /// Asks `healthPath` on the well-known port (non-blocking).
    void probeExistingServer();
    /// Arms a short re-probe: another process is launching this kind right now.
    void scheduleProbeRetry();
    /// Takes the interprocess start lock (`waitMs`; 0 = do not wait).
    bool acquireStartLock(int waitMs, QString* error);
    /// Releases the start lock when this instance holds it (idempotent).
    void releaseStartLock();
    /// Removes our records under the lock (real pid and nothing else).
    void forgetRecords();
    /// Probe answer: 200/503 → reuse, anything else → our own start.
    void onProbeReply(QNetworkReply* reply);
    /// Adopts the server on `port` (foreign — `stop()` will not kill it).
    void adoptExistingServer(int port, bool ready);
    /// Picks a port, builds the arguments and spawns the `QProcess` (no waits).
    void launch(int firstPort);
    /// Asks `healthPath` right away and starts the polls.
    void beginHealthPolling();
    /// One `/health` poll (skipped while the previous one is still in flight).
    void pollHealth();
    /// Handles the `/health` answer.
    void onHealthReply(QNetworkReply* reply);
    /// The process ended (normally or by crash).
    void onProcessFinished(int exitCode, bool crashed);
    /// Reads the available stdout/stderr and turns it into `logLine`.
    void drainOutput();
    /// Moves to `Failed`, stops our process and emits `failed()`.
    void fail(const QString& reason);
    /// Stops the timers and aborts pending requests (without touching the process).
    void resetRequests();

    LocalServerSpec m_spec;
    State   m_state = State::Stopped;
    int     m_port = 0;              ///< the port actually taken
    int     m_probePort = 0;         ///< the port we ask `/health` on
    int     m_attempts = 0;          ///< how many times we started
    bool    m_stopping = false;      ///< `stop()` was requested — not a failure
    bool    m_owned = false;         ///< the process is ours (`stop()` kills it)
    bool    m_reused = false;        ///< a foreign server is reused (we do not kill it)
    qint64  m_pid = 0;               ///< our process id (for the ownership file)
    QString m_lastError;
    QString m_stderrTail;            ///< the last lines (for the failure message)

    /// Our own process (borrowed from the owner) and its owner.
    QPointer<QProcess>     m_proc;
    ProcessOwner           m_procOwner;
    QNetworkAccessManager* m_net = nullptr;      ///< one per instance
    QNetworkReply*         m_probeReply = nullptr;
    QNetworkReply*         m_healthReply = nullptr;
    QTimer                 m_healthTimer;        ///< poll of `healthPath`
    QTimer                 m_probeRetryTimer;    ///< re-probe while a competitor launches
    QTimer                 m_readyDeadline;      ///< `readyTimeoutMs` ceiling
    QElapsedTimer          m_startedAt;          ///< for the log (how long we waited)

    /// The interprocess lock around the ownership decision (see `start()`); held
    /// only from "read the record" until the record for our process exists.
    QLockFile m_startLock;
};

} // namespace Daqster
