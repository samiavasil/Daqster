#include "test_local_server_service.h"

#include <Servers/LocalServerService.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>

namespace {

using Daqster::LocalServerService;
using Daqster::LocalServerSpec;

/// The path to the stub comes from CMake (`$<TARGET_FILE:local_server_stub>`).
#ifndef LOCAL_SERVER_STUB
#define LOCAL_SERVER_STUB ""
#endif

QString stubBinary()
{
    return QString::fromUtf8(LOCAL_SERVER_STUB);
}

/// A free base port for the range — ephemeral, so 18080–18180 is not touched.
int freePortBase(int span)
{
    for (int attempt = 0; attempt < 20; ++attempt) {
        QTcpServer probe;
        if (!probe.listen(QHostAddress::LocalHost, 0))
            return 0;
        const int base = int(probe.serverPort());
        probe.close();
        if (base + span < 65000)
            return base;
    }
    return 0;
}

/// A spec for the stub: short poll and sensible deadlines, so the test is fast.
LocalServerSpec stubSpec(int base)
{
    LocalServerSpec spec;
    spec.kind             = QStringLiteral("stub-server");
    spec.binary           = stubBinary();
    // Explicit QStringList: on Qt5 the brace list is ambiguous between the
    // QList<QString> and QStringList assignment operators.
    spec.arguments        = QStringList{QStringLiteral("--path"),
                                        QStringLiteral("/health")};
    spec.portRangeStart   = base;
    spec.portRangeEnd     = base + 20;
    spec.healthIntervalMs = 50;
    spec.readyTimeoutMs   = 20000;
    spec.stopGraceMs      = 1000;
    spec.stopKillMs       = 500;
    return spec;
}

/// How many stub processes run right now (-1 = the platform cannot tell).
///
/// This is the proof of "no second process is started": the real process count
/// is checked, not just the class' `isOwned()` flag.
int stubProcessCount()
{
#ifdef Q_OS_UNIX
    const QString name = QFileInfo(stubBinary()).fileName();
    if (name.isEmpty())
        return 0;

    int count = 0;
    const QStringList entries =
        QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        bool isPid = false;
        entry.toLongLong(&isPid);
        if (!isPid)
            continue;

        QFile file(QStringLiteral("/proc/%1/cmdline").arg(entry));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QList<QByteArray> args = file.readAll().split('\0');
        for (int i = 0; i < qMin(2, args.size()); ++i) {
            if (QFileInfo(QString::fromLocal8Bit(args.at(i))).fileName() == name) {
                ++count;
                break;
            }
        }
    }
    return count;
#else
    return -1;   // without /proc there is no way to count the processes
#endif
}

} // namespace

namespace {

/// The ownership records as they are on disk right now.
QJsonArray readOwnershipRecords()
{
    QFile file(LocalServerService::ownershipFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll())
        .object().value(QStringLiteral("servers")).toArray();
}

/// Writes one record (what a crashed/older process would have left behind).
void writeOwnershipRecord(const QString& kind, qint64 pid, const QString& binary, int port)
{
    QJsonObject record;
    record.insert(QStringLiteral("kind"), kind);
    record.insert(QStringLiteral("pid"), double(pid));
    record.insert(QStringLiteral("binary"), binary);
    record.insert(QStringLiteral("port"), port);

    QJsonObject root;
    root.insert(QStringLiteral("servers"), QJsonArray{record});

    QFile file(LocalServerService::ownershipFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
}

/// The record for `kind` (empty object when there is none).
QJsonObject recordForKind(const QString& kind)
{
    for (const QJsonValue& value : readOwnershipRecords()) {
        if (value.toObject().value(QStringLiteral("kind")).toString() == kind)
            return value.toObject();
    }
    return {};
}

/// A pid nobody can have (above `pid_max` on Linux, dead everywhere else).
constexpr qint64 kDeadPid = 999999999;

#ifdef Q_OS_UNIX
/// Moves `QStandardPaths::GenericDataLocation` into `path` for its scope.
///
/// The data directory of the service is
/// `<GenericDataLocation>/Daqster`; on Linux that location honours
/// `XDG_DATA_HOME` (read on every call, not only at first use), so a temporary
/// home is enough — no test mode, no touching the real `~/.local/share`.
class ScopedDataHome
{
public:
    explicit ScopedDataHome(const QString& path)
        : m_hadPrevious(qEnvironmentVariableIsSet("XDG_DATA_HOME"))
        , m_previous(qgetenv("XDG_DATA_HOME"))
    {
        qputenv("XDG_DATA_HOME", QFile::encodeName(path));
    }

    ~ScopedDataHome() { restore(); }

    ScopedDataHome(const ScopedDataHome&) = delete;
    ScopedDataHome& operator=(const ScopedDataHome&) = delete;

    /// Puts the previous value back (the destructor calls it; explicit calls are
    /// allowed so the directory can be inspected before the guard dies).
    void restore()
    {
        if (m_hadPrevious)
            qputenv("XDG_DATA_HOME", m_previous);
        else
            qunsetenv("XDG_DATA_HOME");
    }

private:
    bool       m_hadPrevious;
    QByteArray m_previous;
};
#endif

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Isolation: the ownership file is left as it was found
// ═══════════════════════════════════════════════════════════════════════
void TestLocalServerService::init()
{
    QFile file(LocalServerService::ownershipFilePath());
    m_hadOwnershipFile = file.open(QIODevice::ReadOnly);
    m_ownershipBackup  = m_hadOwnershipFile ? file.readAll() : QByteArray();
    file.close();

    // The test starts from a clean state: a stale record from a previous run
    // must not move the probe to a foreign port.
    QFile::remove(LocalServerService::ownershipFilePath());
    // …and a lock file left by a crashed run would make every start wait 10 s.
    QFile::remove(LocalServerService::lockFilePath());
}

void TestLocalServerService::cleanup()
{
    if (!m_hadOwnershipFile) {
        QFile::remove(LocalServerService::ownershipFilePath());
        return;
    }
    QFile restore(LocalServerService::ownershipFilePath());
    if (restore.open(QIODevice::WriteOnly | QIODevice::Truncate))
        restore.write(m_ownershipBackup);
}

// ═══════════════════════════════════════════════════════════════════════
//  1. Start → readiness → ownership
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::startsStubAndBecomesReady()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range — the environment is broken");

    LocalServerService service;
    QSignalSpy started(&service, &LocalServerService::started);
    QSignalSpy failed(&service, &LocalServerService::failed);

    QString error;
    service.start(stubSpec(base), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // `start()` returned immediately — readiness arrives later, without blocking.
    QCOMPARE(service.state(), LocalServerService::State::Starting);

    QTRY_VERIFY_WITH_TIMEOUT(started.count() == 1, 20000);
    QCOMPARE(failed.count(), 0);
    QVERIFY(service.isReady());
    QVERIFY(service.isOwned());
    QVERIFY(!service.isReused());

    // The first free port in the range, not a random one.
    QCOMPARE(service.port(), base);
    QCOMPARE(started.at(0).at(0).toInt(), base);
    QCOMPARE(service.baseUrl(), QUrl(QStringLiteral("http://127.0.0.1:%1").arg(base)));

    if (stubProcessCount() >= 0) {
        QCOMPARE(stubProcessCount(), 1);
        qInfo().noquote() << QStringLiteral("stub processes after start: %1")
                                 .arg(stubProcessCount());
    }

    service.stop();
    QVERIFY(!service.isReady());
    QVERIFY(!service.isOwned());
    QCOMPARE(service.state(), LocalServerService::State::Stopped);
    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  2. Reuse: a second process is NOT started
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::secondStartReusesRunningServer()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    LocalServerService owner;
    owner.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(owner.isReady(), 20000);
    const int port = owner.port();
    QVERIFY(owner.isOwned());
    if (stubProcessCount() >= 0)
        QCOMPARE(stubProcessCount(), 1);

    // A second `start()` on the SAME object is a no-op — no second process.
    owner.start(stubSpec(base));
    QVERIFY(owner.isReady());
    QVERIFY(owner.isOwned());
    QCOMPARE(owner.port(), port);

    // A second instance ("a second scene") finds the running server and reuses it.
    LocalServerService second;
    QSignalSpy started(&second, &LocalServerService::started);
    QSignalSpy failed(&second, &LocalServerService::failed);
    second.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(second.isReady(), 20000);

    QCOMPARE(failed.count(), 0);
    QCOMPARE(started.count(), 1);
    QCOMPARE(started.at(0).at(0).toInt(), port);
    QCOMPARE(second.port(), port);
    QVERIFY(second.isReused());
    QVERIFY2(!second.isOwned(), "a reused server is not ours");

    // The proof: still ONE process, not two.
    if (stubProcessCount() >= 0) {
        const int count = stubProcessCount();
        qInfo().noquote() << QStringLiteral("stub processes after reuse: %1")
                                 .arg(count);
        QCOMPARE(count, 1);
    }

    owner.stop();
    QVERIFY(!owner.isReady());
}

// ═══════════════════════════════════════════════════════════════════════
//  2b. Two starts of the same kind at the same time: the lock, not luck
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::secondStartWithSameKindDoesNotLaunchSecondProcess()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    // The race the lock exists for: both instances ask at the same moment, both
    // find no ownership record and both would launch a server — one on `base`,
    // the next on `base + 1`.
    LocalServerService first;
    LocalServerService second;
    QSignalSpy failedFirst(&first, &LocalServerService::failed);
    QSignalSpy failedSecond(&second, &LocalServerService::failed);

    first.start(stubSpec(base));
    second.start(stubSpec(base));

    QTRY_VERIFY_WITH_TIMEOUT(first.isReady(), 20000);
    QTRY_VERIFY_WITH_TIMEOUT(second.isReady(), 20000);
    QCOMPARE(failedFirst.count(), 0);
    QCOMPARE(failedSecond.count(), 0);

    // Exactly one of them owns a process; the other one reuses it on its port.
    QVERIFY2(first.isOwned() != second.isOwned(),
             "one start must own the process and the other one must reuse it");
    QVERIFY2(first.isReused() != second.isReused(), "exactly one reuses");
    QCOMPARE(first.port(), second.port());
    QCOMPARE(first.port(), base);

    // The proof: one process, not two.
    if (stubProcessCount() >= 0) {
        const int count = stubProcessCount();
        qInfo().noquote() << QStringLiteral("stub processes after two starts: %1").arg(count);
        QCOMPARE(count, 1);
    }

    first.stop();
    second.stop();
    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  2c. A record with a dead pid: removed, replaced, one process
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::staleRecordIsRemovedAndReplaced()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    // What a crash leaves behind: a record for a process that no longer exists.
    // Reading it as "a server is starting" would make the start wait forever.
    writeOwnershipRecord(QStringLiteral("stub-server"), kDeadPid, stubBinary(), base);
    QVERIFY(!recordForKind(QStringLiteral("stub-server")).isEmpty());

    LocalServerService service;
    QSignalSpy started(&service, &LocalServerService::started);
    QSignalSpy failed(&service, &LocalServerService::failed);

    service.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(started.count() == 1, 20000);
    QCOMPARE(failed.count(), 0);
    QVERIFY(service.isOwned());
    QVERIFY2(!service.isReused(), "a dead pid must not be reused");

    // The stale record is gone and the record now names the live process.
    const QJsonObject record = recordForKind(QStringLiteral("stub-server"));
    QVERIFY2(!record.isEmpty(), "the new process must be recorded");
    const qint64 pid = qint64(record.value(QStringLiteral("pid")).toDouble());
    QVERIFY2(pid != kDeadPid, "the stale record was not replaced");
    QVERIFY2(pid > 0, "the record has no pid");
    QCOMPARE(record.value(QStringLiteral("port")).toInt(), service.port());
    if (stubProcessCount() >= 0)
        QCOMPARE(stubProcessCount(), 1);

    service.stop();
    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  2d. The lock is not left behind
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::lockIsReleasedAfterStart()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    // The lock lives next to the ownership file — one directory, one location.
    QCOMPARE(QFileInfo(LocalServerService::lockFilePath()).absolutePath(),
             QFileInfo(LocalServerService::ownershipFilePath()).absolutePath());

    LocalServerService service;
    service.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 20000);

    // A second process (here: a second QLockFile) can take it immediately —
    // holding it through the `/health` wait would block every other start.
    {
        QLockFile probe(LocalServerService::lockFilePath());
        QVERIFY2(probe.tryLock(2000),
                 "the start lock is still held after the server became ready");
        probe.unlock();
    }

    service.stop();
    {
        QLockFile probe(LocalServerService::lockFilePath());
        QVERIFY2(probe.tryLock(2000), "the start lock is still held after stop()");
        probe.unlock();
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  2e. The data directory exists before anything is written in it
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::startCreatesMissingDataDir()
{
#ifndef Q_OS_UNIX
    QSKIP("XDG_DATA_HOME does not drive QStandardPaths on this platform");
#else
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    QTemporaryDir home;
    QVERIFY2(home.isValid(), "no temporary home");
    const QString dataDir = home.path() + QStringLiteral("/Daqster");
    QVERIFY2(!QFileInfo::exists(dataDir), "the premise: <data>/Daqster must be missing");

    ScopedDataHome scopedHome(home.path());

    LocalServerService service;
    QVERIFY2(LocalServerService::ownershipFilePath().startsWith(home.path()),
             qPrintable(LocalServerService::ownershipFilePath()));

    QSignalSpy started(&service, &LocalServerService::started);
    QSignalSpy failed(&service, &LocalServerService::failed);

    QString error;
    service.start(stubSpec(base), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY2(QFileInfo(dataDir).isDir(),
             "start() must create <data>/Daqster before taking the lock");

    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 20000);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(started.count(), 1);
    QVERIFY2(QFileInfo(LocalServerService::lockFilePath()).absolutePath() == dataDir,
             "the lock must live in the directory that was just created");

    service.stop();
#endif
}

void TestLocalServerService::unwritableDataDirReportsPath()
{
#ifndef Q_OS_UNIX
    QSKIP("XDG_DATA_HOME does not drive QStandardPaths on this platform");
#else
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    QTemporaryDir parent;
    QVERIFY2(parent.isValid(), "no temporary directory");
    // A **file** where the directory must be: `mkpath` cannot create it.
    const QString blockedDir = parent.path() + QStringLiteral("/Daqster");
    QFile blocker(blockedDir);
    QVERIFY2(blocker.open(QIODevice::WriteOnly), "cannot create the blocking file");
    blocker.write("not a directory");
    blocker.close();

    ScopedDataHome scopedHome(parent.path());

    LocalServerService service;
    QSignalSpy failed(&service, &LocalServerService::failed);

    QString error;
    service.start(stubSpec(base), &error);

    QVERIFY2(!error.isEmpty(),
             "a data directory that cannot be created must fail, not start opaquely");
    QVERIFY2(error.contains(blockedDir), qPrintable(error));
    QCOMPARE(failed.count(), 1);
    QCOMPARE(service.state(), LocalServerService::State::Failed);
    if (stubProcessCount() >= 0)
        QCOMPARE(stubProcessCount(), 0);   // nothing is launched without the directory
#endif
}

// ═══════════════════════════════════════════════════════════════════════
//  2f. `stop()` removes the record under the lock
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::stopRemovesRecordUnderLock()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    LocalServerService service;
    service.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 20000);
    QVERIFY2(!recordForKind(QStringLiteral("stub-server")).isEmpty(),
             "a running server must be recorded");

    service.stop();

    // The record is gone (so the next start launches instead of reusing a dead
    // pid), and the lock is free — the removal happened under it, briefly.
    QVERIFY2(recordForKind(QStringLiteral("stub-server")).isEmpty(),
             "stop() left the ownership record behind");
    QLockFile probe(LocalServerService::lockFilePath());
    QVERIFY2(probe.tryLock(2000), "stop() left the start lock held");
    probe.unlock();

    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  3. `stop()` does not kill a foreign (reused) server
// ═══════════════════════════════════════════════════════════════════════
void TestLocalServerService::stopLeavesReusedServerAlone()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    LocalServerService owner;
    owner.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(owner.isReady(), 20000);

    LocalServerService guest;
    guest.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(guest.isReady(), 20000);
    QVERIFY(guest.isReused());

    guest.stop();
    QVERIFY(!guest.isReused());   // the connection is closed…
    QVERIFY(owner.isReady());     // …but the owner's server still runs
    if (stubProcessCount() >= 0)
        QCOMPARE(stubProcessCount(), 1);

    owner.stop();
    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  4. Missing / non-executable binary
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::missingBinaryFailsClearly()
{
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    LocalServerService service;
    QSignalSpy failed(&service, &LocalServerService::failed);

    LocalServerSpec spec = stubSpec(base);
    spec.kind   = QStringLiteral("sd-server");
    spec.binary = QStringLiteral("/nonexistent/path/sd-server");

    QString error;
    service.start(spec, &error);

    // A clear reason that names **which file** is missing — not "something
    // went wrong".
    QVERIFY2(!error.isEmpty(), "no message about the missing binary");
    QVERIFY2(error.contains(QStringLiteral("/nonexistent/path/sd-server")),
             qPrintable(error));
    QVERIFY2(error.contains(QStringLiteral("not found")), qPrintable(error));
    QVERIFY2(error.contains(QStringLiteral("sd-server")), qPrintable(error));
    QCOMPARE(service.lastError(), error);
    QCOMPARE(service.state(), LocalServerService::State::Failed);
    QVERIFY(!service.isReady());
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), error);

    // An existing but non-executable file is also a clear failure (and not a
    // crash).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString plain = dir.filePath(QStringLiteral("sd-server"));
    QFile file(plain);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not a program\n");
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    LocalServerService notExecutable;
    QSignalSpy failedNotExecutable(&notExecutable, &LocalServerService::failed);
    LocalServerSpec notExecSpec = stubSpec(base);
    notExecSpec.binary = plain;
    QString notExecError;
    notExecutable.start(notExecSpec, &notExecError);
    QVERIFY2(!notExecError.isEmpty(), "the non-executable file failed silently");
    QVERIFY2(notExecError.contains(QStringLiteral("not an executable file")),
             qPrintable(notExecError));
    QCOMPARE(failedNotExecutable.count(), 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  5. The process dies while running
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::crashAfterReadyIsReported()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    LocalServerService service;
    QSignalSpy started(&service, &LocalServerService::started);
    QSignalSpy failed(&service, &LocalServerService::failed);

    // The stub becomes ready and exits by itself 1.2 s later — "the server died".
    LocalServerSpec spec = stubSpec(base);
    spec.arguments << QStringLiteral("--exit-after-ms") << QStringLiteral("1200");
    service.start(spec);

    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 20000);
    QCOMPARE(started.count(), 1);

    QTRY_VERIFY_WITH_TIMEOUT(failed.count() == 1, 20000);
    QVERIFY2(!service.isReady(), "a dead server must not look ready");
    QCOMPARE(service.state(), LocalServerService::State::Failed);
    QVERIFY(!service.isOwned());

    const QString reason = failed.at(0).at(0).toString();
    QVERIFY2(reason.contains(QStringLiteral("died")), qPrintable(reason));
    QVERIFY2(reason.contains(QStringLiteral("code")), qPrintable(reason));
    QCOMPARE(service.lastError(), reason);

    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);

    service.stop();   // after a failure `stop()` does nothing and does not crash
    QCOMPARE(service.state(), LocalServerService::State::Failed);
}

// ═══════════════════════════════════════════════════════════════════════
//  6. `stop()` before readiness (the invariant from REQ-SW-PL-050)
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::stopBeforeStartedIsNoOp()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");

    // (a) Never started: `stop()` does nothing.
    {
        LocalServerService fresh;
        fresh.stop();
        fresh.stop();
        QCOMPARE(fresh.state(), LocalServerService::State::Stopped);
        QCOMPARE(fresh.port(), 0);
        QVERIFY(!fresh.isReady());
    }

    // (b) Our own process that never answers `/health`: `stop()` arrives while
    //     the process is still `Starting`, i.e. before `started(port)`.
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    LocalServerService service;
    QSignalSpy started(&service, &LocalServerService::started);
    LocalServerSpec spec = stubSpec(base);
    spec.arguments = QStringList{QStringLiteral("--path"), QStringLiteral("/other")};   // not /health
    service.start(spec);

    QTRY_VERIFY_WITH_TIMEOUT(service.isOwned(), 20000);   // the process is ours…
    QVERIFY2(!service.isReady(), "the stub must not answer /health");

    service.stop();   // …and exactly here: `ProcessOwner` sends no signal before exec
    QCOMPARE(started.count(), 0);
    QCOMPARE(service.state(), LocalServerService::State::Stopped);
    QVERIFY(!service.isOwned());
    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  7. Idempotence
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::stopIsIdempotent()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    LocalServerService service;
    service.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 20000);

    service.stop();
    service.stop();
    service.stop();

    QCOMPARE(service.state(), LocalServerService::State::Stopped);
    QVERIFY(!service.isReady());
    QVERIFY(!service.isOwned());
    QVERIFY(!service.isReused());
    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);

    // After a stop the same object can start again (the life is not one-shot).
    service.start(stubSpec(base));
    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 20000);
    QVERIFY(service.isOwned());
    service.stop();
    if (stubProcessCount() >= 0)
        QTRY_COMPARE(stubProcessCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  8. Port selection skips a taken port
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::pickFreePortSkipsBusyPort()
{
    QTcpServer busy;
    QVERIFY(busy.listen(QHostAddress::LocalHost, 0));
    const int taken = int(busy.serverPort());
    QVERIFY(taken > 0);

    const int picked = LocalServerService::pickFreePort(taken, taken + 50);
    QVERIFY2(picked != 0, "no free port in the range — the environment is broken");
    QVERIFY2(picked != taken,
             qPrintable(QStringLiteral("picked the taken port %1").arg(taken)));
    QVERIFY(picked > taken);

    // An empty/invalid range yields no port (not "the first one found").
    QCOMPARE(LocalServerService::pickFreePort(taken, taken), 0);
    busy.close();
}

// ═══════════════════════════════════════════════════════════════════════
//  9. Configurable host/port flags (REQ-SW-PL-081)
// ═══════════════════════════════════════════════════════════════════════

void TestLocalServerService::customPortFlagsAreUsed()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range — the environment is broken");

    LocalServerService service;
    QSignalSpy started(&service, &LocalServerService::started);
    QSignalSpy failed(&service, &LocalServerService::failed);

    LocalServerSpec spec = stubSpec(base);
    // `sd-server` takes `--listen-ip`/`--listen-port` and exits on unknown
    // arguments — the stub is equally strict, so this fails if `--host`/
    // `--port` is passed.
    spec.portFlags = QStringList{QStringLiteral("--listen-ip"),
                                 QStringLiteral("--listen-port")};

    QString error;
    service.start(spec, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QTRY_VERIFY_WITH_TIMEOUT(started.count() == 1, 20000);
    QCOMPARE(failed.count(), 0);
    QVERIFY(service.isReady());
    QCOMPARE(service.port(), base);

    service.stop();
    QVERIFY(!service.isReady());
}

void TestLocalServerService::invalidPortFlagsAreRejected()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");
    const int base = freePortBase(32);
    QVERIFY2(base > 0, "no free port for the range");

    const QList<QStringList> invalid = {
        QStringList{QStringLiteral("--host")},                          // one entry
        QStringList{QStringLiteral("--host"), QStringLiteral("--port"),
                    QStringLiteral("--extra")},                         // three entries
        QStringList{QStringLiteral("--same"), QStringLiteral("--same")},// equal values
        QStringList{QString(), QString()},                              // empty values
    };

    for (const QStringList& flags : invalid) {
        LocalServerService service;
        QSignalSpy failed(&service, &LocalServerService::failed);

        LocalServerSpec spec = stubSpec(base);
        spec.portFlags = flags;
        QString error;
        service.start(spec, &error);

        QVERIFY2(!error.isEmpty(),
                 qPrintable(QStringLiteral("no error for portFlags %1")
                                .arg(flags.join(QLatin1Char(' ')))));
        QVERIFY2(error.contains(QStringLiteral("portFlags")), qPrintable(error));
        QCOMPARE(service.state(), LocalServerService::State::Failed);
        QCOMPARE(failed.count(), 1);
        QVERIFY(!service.isReady());
        QVERIFY(!service.isOwned());
    }
}

void TestLocalServerService::defaultFlagsUnchanged()
{
    if (stubBinary().isEmpty())
        QSKIP("LOCAL_SERVER_STUB is not set (not built through CMake?)");

    const LocalServerSpec spec;
    QCOMPARE(spec.portFlags, (QStringList{QStringLiteral("--host"),
                                          QStringLiteral("--port")}));

    // An explicit `--port` in the arguments would be appended a second time —
    // rejected instead of silently handing the server two ports.
    LocalServerService service;
    QSignalSpy failed(&service, &LocalServerService::failed);
    LocalServerSpec doubled = stubSpec(freePortBase(32));
    doubled.arguments << QStringLiteral("--port") << QStringLiteral("1");
    QString error;
    service.start(doubled, &error);
    QVERIFY2(!error.isEmpty(), "a repeated --port was accepted");
    QVERIFY2(error.contains(QStringLiteral("--port")), qPrintable(error));
    QCOMPARE(failed.count(), 1);

    // …and the same for a custom pair that the caller passes itself.
    LocalServerService custom;
    LocalServerSpec customSpec = stubSpec(freePortBase(32));
    customSpec.portFlags = QStringList{QStringLiteral("--listen-ip"),
                                       QStringLiteral("--listen-port")};
    customSpec.arguments << QStringLiteral("--listen-port") << QStringLiteral("1");
    QString customError;
    custom.start(customSpec, &customError);
    QVERIFY2(customError.contains(QStringLiteral("--listen-port")),
             qPrintable(customError));
}
