#include "Servers/LocalServerService.h"

#include "Threading/ThreadingLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTcpServer>
#include <QThread>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#endif

namespace Daqster {

// ── The ownership file ───────────────────────────────────────────────────────
//
// One JSON for **all** kinds of servers (`kind` is the field that tells them
// apart): `{"servers":[{kind,pid,binary,port},…]}`. The kind is data, not a
// file name — otherwise every new server wants a new file and a new reader.
//
// The record is written on `QProcess::started` (when the OS hands out a
// process id) and removed on exit, and only for **our** pid. It is the second
// source for orphans and it says which port to ask `/health` on when
// `preferredPort == 0`: the first server may have come up on 18081 because
// 18080 was taken — without the record the second start would look at 18080
// and spawn a second process.
namespace {

constexpr int kProbeTimeoutMs   = 800;    ///< ceiling on the "is there a server" probe
constexpr int kMaxStartAttempts = 3;      ///< attempts before the start is a failure
constexpr int kStderrTailLines  = 4;      ///< lines kept for the failure message
constexpr int kOwnerDepth       = 4;      ///< levels up to look for a live owner
constexpr int kSweepGraceMs     = 500;    ///< grace when sweeping an orphan
constexpr int kSweepPollMs      = 25;     ///< step of that wait
constexpr int kStartLockWaitMs  = 10000;  ///< `start()` waits this long for the decision
constexpr int kShortLockWaitMs  = 1000;   ///< `stop()`/`sweepOrphans()` — brief, never a stall
constexpr int kProbeRetryMs     = 200;    ///< a competitor is launching — ask again

QString dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/Daqster");
}

/// The interprocess lock that makes "read the record → decide → launch → record"
/// one step between processes.
QString ownershipLockFilePath()
{
    return dataDir() + QStringLiteral("/local-servers.lock");
}

/// Creates `<GenericDataLocation>/Daqster` before anything is written in it.
///
/// On a fresh machine the directory does not exist: `QLockFile` then refuses
/// with "lock file is not writable" and the ownership file cannot be written at
/// all. One `mkpath` covers the lock, the record and the sweep.
///
/// @param error receives the reason, naming the path, when creation fails
/// @return true when the directory exists afterwards
bool ensureDataDir(QString* error)
{
    const QString dir = dataDir();
    if (QDir().mkpath(dir))
        return true;

    const QString reason =
        QStringLiteral("не може да се създаде директорията за ключалката \"%1\"").arg(dir);
    if (error)
        *error = reason;
    qCWarning(lcThreading).noquote() << QStringLiteral("LocalServerService: %1").arg(reason);
    return false;
}

QJsonArray readRecords()
{
    QFile file(LocalServerService::ownershipFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll())
        .object().value(QStringLiteral("servers")).toArray();
}

void writeRecords(const QJsonArray& records)
{
    if (records.isEmpty()) {
        QFile::remove(LocalServerService::ownershipFilePath());
        return;
    }
    if (!ensureDataDir(nullptr))
        return;

    QJsonObject root;
    root.insert(QStringLiteral("servers"), records);
    QFile file(LocalServerService::ownershipFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

/// Writes (or replaces) the record for `pid` — one process, one record.
void rememberServer(qint64 pid, const LocalServerSpec& spec, int port)
{
    if (pid <= 0)
        return;
    QJsonArray kept;
    for (const QJsonValue& value : readRecords()) {
        if (qint64(value.toObject().value(QStringLiteral("pid")).toDouble()) != pid)
            kept.append(value);
    }
    QJsonObject record;
    record.insert(QStringLiteral("pid"), double(pid));
    record.insert(QStringLiteral("kind"), spec.kind);
    record.insert(QStringLiteral("binary"), spec.binary);
    record.insert(QStringLiteral("port"), port);
    kept.append(record);
    writeRecords(kept);
}

/// Removes the record **only** for `pid` (another instance is left alone).
void forgetServer(qint64 pid)
{
    if (pid <= 0)
        return;
    QJsonArray kept;
    bool removed = false;
    for (const QJsonValue& value : readRecords()) {
        if (qint64(value.toObject().value(QStringLiteral("pid")).toDouble()) == pid) {
            removed = true;
            continue;
        }
        kept.append(value);
    }
    if (removed)
        writeRecords(kept);
}

/// The last recorded port for this kind (0 when there is no record).
int recordedPortFor(const QString& kind)
{
    const QJsonArray records = readRecords();
    for (int i = records.size() - 1; i >= 0; --i) {
        const QJsonObject record = records.at(i).toObject();
        if (record.value(QStringLiteral("kind")).toString() != kind)
            continue;
        const int port = record.value(QStringLiteral("port")).toInt();
        if (port > 0)
            return port;
    }
    return 0;
}

/// The last `count` lines of a text block (for a compact message).
QString tailLines(const QString& text, int count)
{
    QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    while (lines.size() > count)
        lines.removeFirst();
    return lines.join(QStringLiteral(" | ")).trimmed();
}

} // namespace

#ifdef Q_OS_UNIX
namespace {

/// The process arguments from `/proc/<pid>/cmdline` (empty when unreadable).
QStringList readCmdline(qint64 pid)
{
    QFile file(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (pid <= 0 || !file.open(QIODevice::ReadOnly))
        return {};
    QStringList args;
    for (const QByteArray& part : file.readAll().split('\0')) {
        if (!part.isEmpty())
            args << QString::fromLocal8Bit(part);
    }
    return args;
}

/// Is the process alive (`kill(pid, 0)`; `EPERM` also means alive)?
bool isAlive(qint64 pid)
{
    if (pid <= 0)
        return false;
    if (::kill(pid_t(pid), 0) == 0)
        return true;
    return errno == EPERM;
}

/// Parent pid from `/proc/<pid>/stat` (-1 when it cannot be read).
///
/// The format is `pid (comm) state ppid …`, and `comm` may contain spaces and
/// brackets — hence the cut after the **last** closing bracket.
qint64 parentPid(qint64 pid)
{
    QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
    if (pid <= 0 || !file.open(QIODevice::ReadOnly))
        return -1;

    const QByteArray stat = file.readAll();
    const int close = stat.lastIndexOf(')');
    if (close < 0)
        return -1;
    const QList<QByteArray> rest = stat.mid(close + 2).split(' ');
    if (rest.size() < 2)
        return -1;

    bool ok = false;
    const qint64 ppid = rest.at(1).toLongLong(&ok);
    return ok ? ppid : -1;
}

/// Is the live `pid` really this executable (not a recycled pid)?
///
/// `argv[1]` is checked too: for a `#!/bin/sh` script `argv[0]` is `sh` and the
/// file name is the first argument.
bool matchesBinary(qint64 pid, const QString& binaryName)
{
    if (binaryName.isEmpty())
        return false;
    const QStringList args = readCmdline(pid);
    for (int i = 0; i < qMin(2, args.size()); ++i) {
        if (QFileInfo(args.at(i)).fileName().compare(binaryName, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

/// The value after `portFlag` in the process command line (0 when absent).
int portArgument(qint64 pid, const QString& portFlag)
{
    const QStringList args = readCmdline(pid);
    const int index = args.indexOf(portFlag);
    return (index >= 0 && index + 1 < args.size()) ? args.at(index + 1).toInt() : 0;
}

/**
 * @brief Is the live owner of this process one of ours?
 *
 * When **this** process or `Daqster`/`NodeRunner` appears anywhere up the parent
 * chain, the owner is alive and the process is not touched. Otherwise the
 * parent died and the process was re-parented to `init`/`systemd --user`, so
 * nobody will stop it — that is an orphan.
 *
 * Our own pid is checked explicitly: `NodeRunner` and the GUI carry the
 * application name, but a test binary does not, and without that check the
 * tests would sweep their own live server as an "orphan".
 */
bool chainIsAliveOwner(qint64 pid)
{
    const qint64 self = qint64(QCoreApplication::applicationPid());
    qint64 current = pid;
    for (int level = 0; level < kOwnerDepth; ++level) {
        const qint64 ppid = parentPid(current);
        if (ppid <= 1)
            return false;   // reached init/systemd → the parent is gone
        if (ppid == self)
            return true;    // our own instance is alive

        const QStringList args = readCmdline(ppid);
        if (args.isEmpty())
            return false;   // the parent no longer exists
        const QString name = QFileInfo(args.first()).fileName();
        if (name.contains(QLatin1String("daqster"), Qt::CaseInsensitive)
            || name.contains(QLatin1String("NodeRunner")))
            return true;
        current = ppid;
    }
    return false;
}

/// Polite stop: SIGTERM → wait up to `graceMs` → SIGKILL.
bool terminatePid(qint64 pid, int graceMs)
{
    if (pid <= 0 || !isAlive(pid))
        return true;

    ::kill(pid_t(pid), SIGTERM);
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < graceMs) {
        if (!isAlive(pid))
            return true;
        QThread::msleep(kSweepPollMs);
    }
    ::kill(pid_t(pid), SIGKILL);
    for (int i = 0; i < 10; ++i) {
        if (!isAlive(pid))
            return true;
        QThread::msleep(kSweepPollMs);
    }
    return !isAlive(pid);
}

} // namespace
#endif  // Q_OS_UNIX

// ── The live record: "somebody is already starting or running this kind" ─────
//
// The ownership file is written when the OS hands out a pid, so a record means
// "starting or running" and nothing else. Reading it is only trustworthy under
// the start lock (see `LocalServerService::start()`): two starts that both read
// an empty file both launch, and the second server takes the next port while the
// first one holds its own.
namespace {

/// What the ownership file says about one `kind` (and whether it still holds).
struct OwnershipRecord
{
    qint64 pid  = 0;
    int    port = 0;
    bool   live = false;   ///< a process with that pid runs **this** executable
};

/// Is the recorded process still the server (pids are recycled, `binary` is the
/// only proof)? Without `/proc` the answer is "no" — the probe still reuses a
/// server that answers `healthPath`, so nothing is lost but the early reuse.
bool recordProcessIsLive(qint64 pid, const QString& binaryName)
{
#ifdef Q_OS_UNIX
    return isAlive(pid) && matchesBinary(pid, binaryName);
#else
    Q_UNUSED(pid);
    Q_UNUSED(binaryName);
    return false;
#endif
}

/**
 * @brief The live record for `kind`; stale records are removed on the way.
 *
 * Stale = the pid is gone, or the process with that pid is a different
 * executable. Such a record must be dropped, otherwise the next start keeps
 * looking at a port nobody listens on.
 *
 * Must be called **under the start lock**: it writes the file when it removes a
 * stale record.
 *
 * @param kind       server kind (`whisper-server`, `piper`, …)
 * @param binaryName file name of the executable the record must match
 * @return the first live record (empty, `live == false`, when there is none)
 */
OwnershipRecord liveRecordFor(const QString& kind, const QString& binaryName)
{
    OwnershipRecord found;
    QJsonArray kept;
    bool removed = false;

    for (const QJsonValue& value : readRecords()) {
        const QJsonObject record = value.toObject();
        if (record.value(QStringLiteral("kind")).toString() != kind) {
            kept.append(value);
            continue;
        }
        const qint64 pid = qint64(record.value(QStringLiteral("pid")).toDouble());
        if (!recordProcessIsLive(pid, binaryName)) {
            removed = true;   // dead pid or a recycled one — not our server
            continue;
        }
        if (!found.live) {
            found.pid  = pid;
            found.port = record.value(QStringLiteral("port")).toInt();
            found.live = found.port > 0;
        }
        kept.append(value);
    }

    if (removed) {
        writeRecords(kept);
        qCInfo(lcThreading).noquote()
            << QStringLiteral("%1: stale ownership record removed (its process is "
                              "gone) — the next start may launch")
                   .arg(kind);
    }
    return found;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Life
// ═══════════════════════════════════════════════════════════════════════

LocalServerService::LocalServerService(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_startLock(ownershipLockFilePath())
{
    connect(&m_healthTimer, &QTimer::timeout, this, &LocalServerService::pollHealth);

    // Another process holds the start lock → it is launching this kind right
    // now. Ask again shortly (never block: its fork/exec is what we wait for).
    m_probeRetryTimer.setSingleShot(true);
    m_probeRetryTimer.setInterval(kProbeRetryMs);
    connect(&m_probeRetryTimer, &QTimer::timeout,
            this, &LocalServerService::probeExistingServer);

    m_readyDeadline.setSingleShot(true);
    connect(&m_readyDeadline, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("%1 did not become ready within %2 ms%3")
                 .arg(m_spec.kind)
                 .arg(m_spec.readyTimeoutMs)
                 .arg(m_stderrTail.isEmpty()
                          ? QStringLiteral(" (no output)")
                          : QStringLiteral(": ") + tailLines(m_stderrTail, kStderrTailLines)));
    });
}

LocalServerService::~LocalServerService()
{
    // The destructor cannot wait for the full grace period (the thread is
    // shutting down), but it must not leave a live server behind — it holds the
    // model in memory. Only an OWN process: a reused one has another owner.
    m_stopping = true;
    resetRequests();
    if (m_owned && m_procOwner.isRunning()) {
        m_procOwner.stop(qMin(m_spec.stopGraceMs, 1000), qMin(m_spec.stopKillMs, 500));
        forgetRecords();
    }
    releaseStartLock();   // never leave a lock file behind a dying process
}

QString LocalServerService::ownershipFilePath()
{
    return dataDir() + QStringLiteral("/local-servers.json");
}

QString LocalServerService::lockFilePath()
{
    return ownershipLockFilePath();
}

QString LocalServerService::hostFlag() const
{
    return m_spec.portFlags.value(0, QStringLiteral("--host"));
}

QString LocalServerService::portFlag() const
{
    return m_spec.portFlags.value(1, QStringLiteral("--port"));
}

bool LocalServerService::portFlagsAreValid(QString* reason) const
{
    const QStringList& flags = m_spec.portFlags;
    if (flags.isEmpty())
        return true;   // the defaults

    QString problem;
    if (flags.size() != 2) {
        problem = QStringLiteral("portFlags must be empty or exactly two entries "
                                 "(host, port), got %1")
                      .arg(flags.size());
    } else if (flags.at(0).isEmpty() || flags.at(1).isEmpty()) {
        problem = QStringLiteral("portFlags entries must not be empty");
    } else if (flags.at(0) == flags.at(1)) {
        problem = QStringLiteral("portFlags host and port flags are the same (\"%1\")")
                      .arg(flags.at(0));
    } else {
        return true;
    }

    if (reason)
        *reason = QStringLiteral("%1: %2").arg(m_spec.kind, problem);
    return false;
}

QStringList LocalServerService::repeatedPortFlags() const
{
    QStringList repeated;
    for (const QString& flag : { hostFlag(), portFlag(), QStringLiteral("--host"),
                                 QStringLiteral("--port") }) {
        if (m_spec.arguments.contains(flag) && !repeated.contains(flag))
            repeated << flag;
    }
    return repeated;
}

void LocalServerService::start(const LocalServerSpec& spec, QString* error)
{
    if (error)
        error->clear();
    if (m_state == State::Starting || m_state == State::Ready)
        return;   // already running (or warming up) — no second process

    m_spec      = spec;
    m_stopping  = false;
    m_attempts  = 0;
    m_port      = 0;
    m_probePort = 0;
    m_owned     = false;
    m_reused    = false;
    m_lastError.clear();
    m_stderrTail.clear();

    // The checks are **before** the start and are synchronous: for a missing
    // file there is no point in waiting for a two-minute timeout, and
    // `failed()` has to arrive while the caller is still listening.
    QString reason;
    const QFileInfo binary(m_spec.binary);
    if (m_spec.kind.isEmpty()) {
        reason = QStringLiteral("LocalServerService: missing server kind (kind)");
    } else if (m_spec.binary.isEmpty() || !binary.exists()) {
        reason = QStringLiteral("%1: executable file not found \"%2\"")
                     .arg(m_spec.kind, m_spec.binary);
    } else if (!binary.isFile() || !binary.isExecutable()) {
        reason = QStringLiteral("%1: \"%2\" is not an executable file")
                     .arg(m_spec.kind, m_spec.binary);
    } else if (m_spec.portRangeStart <= 0 || m_spec.portRangeEnd <= m_spec.portRangeStart) {
        reason = QStringLiteral("%1: invalid port range [%2, %3)")
                     .arg(m_spec.kind)
                     .arg(m_spec.portRangeStart)
                     .arg(m_spec.portRangeEnd);
    } else if (!portFlagsAreValid(&reason)) {
        // `reason` is filled by the check itself (one message per defect).
    } else if (const QStringList repeated = repeatedPortFlags(); !repeated.isEmpty()) {
        reason = QStringLiteral("%1: the arguments already contain %2 — the class "
                                "appends the host/port flags itself")
                     .arg(m_spec.kind, repeated.join(QStringLiteral(", ")));
    }
    if (!reason.isEmpty()) {
        m_state     = State::Failed;
        m_lastError = reason;
        if (error)
            *error = reason;
        qCWarning(lcThreading).noquote()
            << QStringLiteral("LocalServerService: %1").arg(reason);
        Q_EMIT failed(reason);
        return;
    }

    // The lock and the ownership file live in the same directory, and on a fresh
    // machine it does not exist yet (REQ-SW-PL-081). `QLockFile` cannot lock
    // inside a missing directory, so the directory is created — and a failure
    // names the path — before the sweep or the lock is reached.
    if (!ensureDataDir(&reason)) {
        m_state     = State::Failed;
        m_lastError = reason;
        if (error)
            *error = reason;
        Q_EMIT failed(reason);
        return;
    }

    // Orphans first (they hold the port and the memory), then the question "is
    // there a server already running that we can reuse". Our own process is
    // spawned only when both steps find nothing.
    m_startedAt.start();
    sweepOrphans();

    // The decision is **one step between processes** (REQ-SW-PL-081): two starts
    // that both read an empty ownership file both launch a server, and the
    // second one takes the next port while the first one holds its own. The lock
    // is released as soon as we know the answer — the `/health` wait below never
    // holds it.
    QString lockReason;
    if (!acquireStartLock(kStartLockWaitMs, &lockReason)) {
        fail(lockReason);
        return;
    }
    const OwnershipRecord record =
        liveRecordFor(m_spec.kind, QFileInfo(m_spec.binary).fileName());
    releaseStartLock();

    if (record.live) {
        // The record is written the moment the OS hands out a pid, so a live
        // record means "starting or running" — somebody else already launched
        // this kind and we must not launch a second process.
        qCInfo(lcThreading).noquote()
            << QStringLiteral("%1: recorded server pid %2 is alive on %3 — reusing "
                              "it, no second process")
                   .arg(m_spec.kind)
                   .arg(record.pid)
                   .arg(record.port);
        adoptExistingServer(record.port, false);
        return;
    }

    probeExistingServer();
}

void LocalServerService::stop()
{
    m_stopping = true;
    resetRequests();
    if (m_state != State::Failed)
        m_state = State::Stopped;

    // A reused server is foreign: we close the connection but do not kill it.
    if (!m_owned || !m_procOwner.isRunning()) {
        if (m_reused) {
            qCInfo(lcThreading).noquote()
                << QStringLiteral("%1: the server on %2 is not ours — leaving it "
                                  "running")
                       .arg(m_spec.kind, baseUrl().toString());
            m_reused = false;
        }
        m_owned = false;
        m_proc  = nullptr;
        releaseStartLock();   // no launch of ours may keep the lock in flight
        return;
    }

    // `ProcessOwner`: terminate → grace → kill → killWait. The deadlines come
    // from the spec, and the answer is ready when `stop()` returns
    // (REQ-SW-PL-050).
    const ProcessOwner::StopResult result =
        m_procOwner.stop(qMax(0, m_spec.stopGraceMs), qMax(0, m_spec.stopKillMs));
    if (result == ProcessOwner::StopResult::Killed) {
        qCWarning(lcThreading)
            << QStringLiteral("%1: did not stop within %2 ms — kill()")
                   .arg(m_spec.kind)
                   .arg(m_spec.stopGraceMs);
    } else if (result == ProcessOwner::StopResult::TimedOut) {
        qCWarning(lcThreading)
            << QStringLiteral("%1: still alive %2 ms after kill()")
                   .arg(m_spec.kind)
                   .arg(m_spec.stopKillMs);
    }

    forgetRecords();   // under the lock — the file is written by every process
    m_owned = false;
    m_proc  = nullptr;   // the owner deletes it with its own `finished`
}

// ═══════════════════════════════════════════════════════════════════════
//  The interprocess start lock
// ═══════════════════════════════════════════════════════════════════════

bool LocalServerService::acquireStartLock(int waitMs, QString* error)
{
    if (error)
        error->clear();
    if (m_startLock.isLocked())
        return true;   // ours already (a launch is in flight)

    // Covers every writer: `start()`/`launch()` (the lock), `sweepOrphans()`
    // and `stop()`/`forgetRecords()` (the record under the same lock).
    if (!ensureDataDir(error))
        return false;

    if (m_startLock.tryLock(waitMs))
        return true;

    const QString reason =
        QStringLiteral("не може да се вземе ключалката за услугата '%1' (%2 s)")
            .arg(m_spec.kind)
            .arg(waitMs / 1000);
    if (error)
        *error = reason;
    qCWarning(lcThreading).noquote()
        << QStringLiteral("LocalServerService: %1 — %2")
               .arg(reason, m_startLock.error() == QLockFile::LockFailedError
                                ? QStringLiteral("another process holds it")
                                : QStringLiteral("the lock file is not writable"));
    return false;
}

void LocalServerService::releaseStartLock()
{
    if (m_startLock.isLocked())
        m_startLock.unlock();
}

void LocalServerService::forgetRecords()
{
    // Two processes must not rewrite the ownership file at the same time, so the
    // removal happens under the same lock. Brief: when somebody else holds it the
    // record stays behind and the next start drops it as stale (dead pid).
    const bool held = acquireStartLock(kShortLockWaitMs, nullptr);
    if (held) {
        if (m_pid > 0)
            forgetServer(m_pid);
        releaseStartLock();
    } else if (m_pid > 0) {
        qCWarning(lcThreading).noquote()
            << QStringLiteral("%1: could not take the ownership lock (%2 s) — the "
                              "record for pid %3 stays and will be dropped as stale")
                   .arg(m_spec.kind)
                   .arg(kShortLockWaitMs / 1000)
                   .arg(m_pid);
    }
    m_pid = 0;
}

void LocalServerService::scheduleProbeRetry()
{
    if (m_probeRetryTimer.isActive())
        return;   // already armed

    // Another process holds the start lock, i.e. it is launching this kind right
    // now. Wait for its server instead of launching a second one — but not for
    // ever: the readiness budget is the ceiling.
    if (m_startedAt.isValid() && m_startedAt.elapsed() >= m_spec.readyTimeoutMs) {
        fail(QStringLiteral("%1: another process is launching the same server, but "
                            "port %2 did not answer %3 within %4 ms")
                 .arg(m_spec.kind)
                 .arg(m_probePort)
                 .arg(m_spec.healthPath)
                 .arg(m_spec.readyTimeoutMs));
        return;
    }
    m_probeRetryTimer.start();
}

// ═══════════════════════════════════════════════════════════════════════
//  Orphans, probe, start
// ═══════════════════════════════════════════════════════════════════════

void LocalServerService::sweepOrphans()
{
#ifdef Q_OS_UNIX
    // The sweep writes the ownership file (it forgets what it stopped), so it
    // takes the same lock as `start()`/`stop()` — briefly, because it only has
    // to beat another process to the file, not to wait for a server.
    if (!ensureDataDir(nullptr))
        return;   // the reason (with the path) is logged by `ensureDataDir()`
    const bool lockedByUs = m_startLock.isLocked();
    if (!lockedByUs && !acquireStartLock(kShortLockWaitMs, nullptr)) {
        qCWarning(lcThreading).noquote()
            << QStringLiteral("%1: could not take the ownership lock (%2 s) — "
                              "skipping the orphan sweep")
                   .arg(m_spec.kind)
                   .arg(kShortLockWaitMs / 1000);
        return;
    }

    const QString binaryName = QFileInfo(m_spec.binary).fileName();
    QSet<qint64> candidates;

    // 1) Live processes of the same executable inside our port range. Only
    //    there: a foreign server on another port is none of our business.
    for (const QString& entry : QDir(QStringLiteral("/proc"))
                                    .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        const qint64 pid = entry.toLongLong(&isPid);
        if (!isPid || !matchesBinary(pid, binaryName))
            continue;
        const int port = portArgument(pid, portFlag());
        if (port < m_spec.portRangeStart || port >= m_spec.portRangeEnd)
            continue;
        candidates.insert(pid);
    }

    // 2) The process recorded in the file — the second source, even when /proc
    //    does not help. Pids are recycled, so the record alone is not proof:
    //    it is verified against the executable name.
    for (const QJsonValue& value : readRecords()) {
        const QJsonObject record = value.toObject();
        if (record.value(QStringLiteral("kind")).toString() != m_spec.kind)
            continue;
        const qint64 pid = qint64(record.value(QStringLiteral("pid")).toDouble());
        if (pid > 0 && matchesBinary(pid, binaryName))
            candidates.insert(pid);
    }

    const qint64 self = qint64(QCoreApplication::applicationPid());
    int swept = 0;
    for (qint64 pid : candidates) {
        if (pid == self || chainIsAliveOwner(pid))
            continue;   // its owner is alive → that owner will stop it
        qCWarning(lcThreading).noquote()
            << QStringLiteral("%1: orphaned process (its parent died) — stopping "
                              "it: pid %2")
                   .arg(m_spec.kind)
                   .arg(pid);
        if (!terminatePid(pid, kSweepGraceMs)) {
            qCWarning(lcThreading).noquote()
                << QStringLiteral("%1: orphaned pid %2 refused to stop — its port "
                                  "stays taken")
                       .arg(m_spec.kind)
                       .arg(pid);
        }
        forgetServer(pid);
        ++swept;
    }

    if (swept > 0) {
        qCInfo(lcThreading).noquote()
            << QStringLiteral("%1: orphaned processes swept: %2")
                   .arg(m_spec.kind)
                   .arg(swept);
    }
#endif
}

void LocalServerService::probeExistingServer()
{
    // Which port is "well-known"? The one fixed by the spec, otherwise the port
    // WE last started a server of this kind on (the file), otherwise the start
    // of the range.
    m_probePort = m_spec.preferredPort;
    if (m_probePort <= 0) {
        m_probePort = recordedPortFor(m_spec.kind);
        if (m_probePort <= 0)
            m_probePort = m_spec.portRangeStart;
    }

    m_state = State::Starting;

    QNetworkRequest request{
        QUrl(QStringLiteral("http://%1:%2%3")
                 .arg(m_spec.host)
                 .arg(m_probePort)
                 .arg(m_spec.healthPath))};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    request.setTransferTimeout(kProbeTimeoutMs);

    QNetworkReply* reply = m_net->get(request);
    m_probeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] { onProbeReply(reply); });
}

void LocalServerService::onProbeReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply != m_probeReply)
        return;   // a late answer from a previous start
    m_probeReply = nullptr;
    if (m_state != State::Starting)
        return;   // `stop()` in the meantime

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 200 = ready; 503 = the server is there but still coming up ("loading").
    if (status == 200 || status == 503) {
        adoptExistingServer(m_probePort, status == 200);
        return;
    }
    launch(m_spec.preferredPort > 0 ? m_spec.preferredPort : m_spec.portRangeStart);
}

void LocalServerService::adoptExistingServer(int port, bool ready)
{
    m_port   = port;
    m_owned  = false;
    m_reused = true;

    qCInfo(lcThreading).noquote()
        << QStringLiteral("%1: reusing the existing server on %2 (%3) — no second "
                          "process; stop() will not shut it down")
               .arg(m_spec.kind,
                    baseUrl().toString(),
                    ready ? QStringLiteral("ready") : QStringLiteral("still starting"));

    if (!ready) {
        // 503 — or a record whose process lives but has not answered yet: the
        // server is there but not accepting, so we wait for readiness the same
        // way we would wait for our own process.
        m_state = State::Starting;
        beginHealthPolling();
        return;
    }

    m_state = State::Ready;
    Q_EMIT started(m_port);
}

int LocalServerService::pickFreePort(int firstPort, int lastPort, const QString& host)
{
    const QHostAddress address(host);
    for (int port = qMax(1, firstPort); port < lastPort; ++port) {
        QTcpServer probe;
        if (!probe.listen(address, quint16(port)))
            continue;
        probe.close();
        return port;
    }
    return 0;
}

QUrl LocalServerService::baseUrl() const
{
    return QUrl(QStringLiteral("http://%1:%2").arg(m_spec.host).arg(m_port));
}

void LocalServerService::launch(int firstPort)
{
    // The lock is taken **before** the question "did another process start this
    // kind while we were probing?" and released when the record for our process
    // exists (`started`) — that is the whole window, milliseconds. 0 ms of wait:
    // a competitor holding it is launching right now, and blocking the GUI thread
    // for its fork/exec would gain nothing; we ask `/health` again shortly.
    if (!m_startLock.isLocked()) {
        if (!acquireStartLock(0, nullptr)) {
            scheduleProbeRetry();
            return;
        }
        const OwnershipRecord record =
            liveRecordFor(m_spec.kind, QFileInfo(m_spec.binary).fileName());
        if (record.live) {
            releaseStartLock();
            qCInfo(lcThreading).noquote()
                << QStringLiteral("%1: another start recorded pid %2 on %3 — reusing "
                                  "it instead of launching")
                       .arg(m_spec.kind)
                       .arg(record.pid)
                       .arg(record.port);
            adoptExistingServer(record.port, false);
            return;
        }
    }

    // Auto-selection continues from the given port: on a retry (lost race for
    // the port) that is the next one, not the same taken port.
    m_port = m_spec.preferredPort > 0
                 ? m_spec.preferredPort
                 : pickFreePort(firstPort, m_spec.portRangeEnd, m_spec.host);
    if (m_port <= 0) {
        fail(QStringLiteral("%1: no free port in the range [%2, %3)")
                 .arg(m_spec.kind)
                 .arg(firstPort)
                 .arg(m_spec.portRangeEnd));
        return;
    }
    m_probePort = m_port;

    QStringList args = m_spec.arguments;
    args << hostFlag() << m_spec.host
         << portFlag() << QString::number(m_port);

    if (m_proc) {
        // An exited process may still live as an object (its
        // `finished → deleteLater` has not been delivered); the owner starts
        // a new one.
        m_proc->disconnect(this);
        m_proc = nullptr;
    }
    m_procOwner.setOwned(true);   // this process is always ours
    if (!m_procOwner.start(m_spec.binary, args)) {
        fail(QStringLiteral("%1: the process could not start (%2)")
                 .arg(m_spec.kind, m_spec.binary));
        return;
    }

    m_proc = m_procOwner.process();
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) {
                drainOutput();
                onProcessFinished(exitCode, status != QProcess::NormalExit);
            });
    connect(m_proc, &QProcess::readyReadStandardOutput,
            this, &LocalServerService::drainOutput);
    connect(m_proc, &QProcess::readyReadStandardError,
            this, &LocalServerService::drainOutput);
    // A failed exec never emits `started`, so the record will never exist —
    // release the lock here, otherwise it would stay until the deadline.
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            releaseStartLock();
    });
    // The record is written as soon as the OS hands out a pid: after a parent
    // crash it is the only thing by which the next start recognises the orphan.
    // It is also the point where the start lock is no longer needed.
    connect(m_proc, &QProcess::started, this, [this] {
        if (!m_proc) {
            releaseStartLock();   // `stop()` in the meantime — nothing to record
            return;
        }
        m_pid = qint64(m_proc->processId());
        rememberServer(m_pid, m_spec, m_port);
        releaseStartLock();
    });

    m_state = State::Starting;
    m_owned = true;   // from here on the process is ours and `stop()` kills it
    ++m_attempts;
    m_stderrTail.clear();

    qCInfo(lcThreading).noquote()
        << QStringLiteral("%1: starting %2 (%3)")
               .arg(m_spec.kind, m_spec.binary, baseUrl().toString());
    beginHealthPolling();   // readiness is polled — never a blocking wait
}

// ═══════════════════════════════════════════════════════════════════════
//  Readiness
// ═══════════════════════════════════════════════════════════════════════

void LocalServerService::beginHealthPolling()
{
    m_startedAt.start();
    m_healthTimer.setInterval(qMax(20, m_spec.healthIntervalMs));
    m_healthTimer.start();
    m_readyDeadline.start(qMax(1000, m_spec.readyTimeoutMs));
    pollHealth();
}

void LocalServerService::pollHealth()
{
    if (m_state != State::Starting) {
        m_healthTimer.stop();
        return;
    }
    if (m_healthReply)
        return;   // the previous poll has not answered yet — do not pile up

    QNetworkRequest request{QUrl(baseUrl().toString() + m_spec.healthPath)};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = m_net->get(request);
    m_healthReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onHealthReply(reply);
    });
}

void LocalServerService::onHealthReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply != m_healthReply)
        return;   // a late answer from a stopped/replaced poll
    m_healthReply = nullptr;
    if (m_state != State::Starting)
        return;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200)
        return;   // 503 = "loading" → the next poll; no answer = not listening yet

    m_healthTimer.stop();
    m_readyDeadline.stop();
    m_state = State::Ready;

    qCInfo(lcThreading).noquote()
        << QStringLiteral("%1: ready in %2 ms (%3)")
               .arg(m_spec.kind)
               .arg(m_startedAt.elapsed())
               .arg(baseUrl().toString());
    Q_EMIT started(m_port);
}

// ═══════════════════════════════════════════════════════════════════════
//  Output and failure
// ═══════════════════════════════════════════════════════════════════════

void LocalServerService::drainOutput()
{
    if (!m_proc)
        return;

    const QString out = QString::fromLocal8Bit(m_proc->readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(m_proc->readAllStandardError());
    for (const QString& chunk : {out, err}) {
        for (const QString& line : chunk.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty())
                continue;
            qCDebug(lcThreading).noquote()
                << QStringLiteral("%1: %2").arg(m_spec.kind, trimmed);
            Q_EMIT logLine(trimmed);
        }
    }
    m_stderrTail += err;
}

void LocalServerService::onProcessFinished(int exitCode, bool crashed)
{
    // The process is gone → the record no longer describes a live process, and
    // the borrowed pointer must not point at an object the owner will delete.
    forgetRecords();
    m_owned = false;
    m_proc  = nullptr;

    if (m_stopping) {
        if (m_state != State::Failed)
            m_state = State::Stopped;
        return;
    }

    const QString detail = m_stderrTail.isEmpty()
                               ? QStringLiteral("no output")
                               : tailLines(m_stderrTail, kStderrTailLines);

    if (m_state == State::Starting && m_attempts < kMaxStartAttempts) {
        // Died before readiness. The usual reason with an auto port is a lost
        // race for the port ("couldn't bind") — the next port is cheaper than
        // failing the whole service.
        qCWarning(lcThreading).noquote()
            << QStringLiteral("%1: exited before readiness (code %2) — retrying "
                              "(%3/%4)")
                   .arg(m_spec.kind)
                   .arg(exitCode)
                   .arg(m_attempts + 1)
                   .arg(kMaxStartAttempts);
        launch(m_port + 1);
        return;
    }

    // After readiness there is no restart: the server lost its model, and the
    // decision "bring it up again" belongs to whoever knows what is in memory.
    fail(QStringLiteral("%1 died %2(code %3): %4")
             .arg(m_spec.kind,
                  crashed ? QStringLiteral("from a crash ") : QString(),
                  QString::number(exitCode),
                  detail));
}

void LocalServerService::fail(const QString& reason)
{
    m_lastError = reason;
    m_state     = State::Failed;
    m_stopping  = true;   // the process' exit is no longer a "crash"
    resetRequests();

    if (m_procOwner.isRunning()) {
        // The same bounded policy as `stop()`: polite, then kill.
        m_procOwner.stop(qMax(0, m_spec.stopGraceMs), qMax(0, m_spec.stopKillMs));
        m_proc = nullptr;
    }
    forgetRecords();
    m_owned = false;

    qCWarning(lcThreading).noquote()
        << QStringLiteral("%1: %2").arg(m_spec.kind, reason);
    Q_EMIT failed(reason);
}

void LocalServerService::resetRequests()
{
    m_healthTimer.stop();
    m_probeRetryTimer.stop();
    m_readyDeadline.stop();

    for (QNetworkReply* reply : {m_probeReply, m_healthReply}) {
        if (!reply)
            continue;
        if (reply == m_probeReply)
            m_probeReply = nullptr;
        else
            m_healthReply = nullptr;
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}

} // namespace Daqster
