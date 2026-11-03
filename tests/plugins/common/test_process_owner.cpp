#include "test_process_owner.h"

#include <Threading/ProcessOwner.h>

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#ifndef Q_OS_WIN
#include <csignal>
#include <ctime>
#include <unistd.h>
#endif

namespace {

using Daqster::ProcessOwner;
using StopResult = ProcessOwner::StopResult;

#ifndef Q_OS_WIN
/// File descriptor the inherited SIGTERM handler writes one byte to.
volatile sig_atomic_t g_preExecMarkerFd = -1;

/// Runs in the *forked copy* when stop() signals before exec().
///
/// Only async-signal-safe work: one write(2). The handler is installed in the
/// test process, so a fork() inherits it; exec() resets caught signals to the
/// default disposition, so a post-exec SIGTERM never reaches this function.
extern "C" void markPreExecSignal(int)
{
    if (g_preExecMarkerFd >= 0) {
        const char byte = 'x';
        const ssize_t written = ::write(g_preExecMarkerFd, &byte, 1);
        (void)written;
    }
}

/// Installs markPreExecSignal for the lifetime of the guard, then restores the
/// previous handler (QtTest's own — the one that turns a stray SIGTERM into
/// `QFATAL "Received signal 15"`).
class ScopedSigtermProbe
{
public:
    explicit ScopedSigtermProbe(int markerFd)
        : m_previous(::signal(SIGTERM, markPreExecSignal))
    {
        g_preExecMarkerFd = markerFd;
    }
    ~ScopedSigtermProbe()
    {
        g_preExecMarkerFd = -1;
        ::signal(SIGTERM, m_previous);
    }

    ScopedSigtermProbe(const ScopedSigtermProbe &) = delete;
    ScopedSigtermProbe &operator=(const ScopedSigtermProbe &) = delete;

private:
    using Handler = void (*)(int);
    Handler m_previous;
};
#endif

/// Path of a `sleep` that lives long enough to be signalled. Not portable on
/// purpose — every test that uses it skips on Windows first.
QString sleepPath()
{
    const QStringList candidates{QStringLiteral("/bin/sleep"), QStringLiteral("/usr/bin/sleep")};
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

/// Wait until `process` has really started (waitForStarted() covers exec).
bool waitStarted(QProcess *process)
{
    return process->state() != QProcess::NotRunning || process->waitForStarted(5000);
}

/// Write an executable POSIX shell script and return its path (empty on error).
///
/// The script creates `ready.flag` in its working directory after installing the
/// SIGTERM trap. The test waits for that file before signalling — without it the
/// SIGTERM can reach the shell before `trap` runs, the child dies from the
/// default disposition and the test would "prove" the kill path with a process
/// that never ignored anything.
QString writeStubbornServer(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("stubborn-server.sh"));
    QFile script(path);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    // `trap '' TERM` + a blocking builtin: ONE process that ignores the polite
    // signal and has no child (so nothing is orphaned when SIGKILL reaps it),
    // and no busy loop. `read` blocks on the stdin pipe QProcess hands it.
    // `exec sleep` would NOT work: the shell resets ignored dispositions on
    // exec, so the exec'ed program dies on SIGTERM (measured).
    script.write("#!/bin/sh\n"
                 "# Fake llama-server: ignores the polite signal (REQ-SW-PL-050 D2).\n"
                 "trap '' TERM\n"
                 ": > ready.flag\n"
                 "read line\n");
    script.close();
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

} // namespace

void ProcessOwnerTest::stopBeforeStartIsNoOp()
{
    ProcessOwner owner;
    QVERIFY(!owner.isRunning());
    QCOMPARE(owner.processId(), 0);
    QCOMPARE(owner.stop(), StopResult::NotRunning);
    QCOMPARE(owner.stop(), StopResult::NotRunning);
}

void ProcessOwnerTest::startRejectsEmptyProgram()
{
    ProcessOwner owner;
    QVERIFY(!owner.start(QString(), QStringList()));
    QVERIFY(!owner.start(QStringLiteral("   "), QStringList()));
    QVERIFY(!owner.isRunning());
}

void ProcessOwnerTest::stopBeforeExecReportsNotRunning()
{
    ProcessOwner owner;
    // start() only queues the spawn; exec then fails because there is no such
    // file. That fork is not ours to signal — stop() must report it and send
    // nothing (a FailedToStart has no `finished`, so a signal would be lost).
    QVERIFY(owner.start(QStringLiteral("/nonexistent/daqster-no-such-program"), {}));
    QCOMPARE(owner.stop(0, 1000), StopResult::NotRunning);
    QVERIFY(!owner.isRunning());
    QCOMPARE(owner.processId(), 0);
    QCOMPARE(owner.stop(), StopResult::NotRunning);
}

void ProcessOwnerTest::stopWhileStartingDoesNotSignalTheChild()
{
#ifdef Q_OS_WIN
    QSKIP("POSIX fixture: the probe is a SIGTERM handler inherited across fork()");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // The handler is inherited by fork() and reset by exec(). A byte in the
    // marker therefore means exactly one thing: stop() signalled the child
    // before it exec'ed — the bug that made QtTest children print
    // `QFATAL "Received signal 15"` and dump core.
    const QString marker = dir.filePath(QStringLiteral("pre-exec-signal.flag"));
    QFile markerFile(marker);
    QVERIFY(markerFile.open(QIODevice::WriteOnly));

    // No trap in the script: after exec the shell keeps the default SIGTERM
    // disposition, so the polite signal ends it without touching the marker.
    const QString script = dir.filePath(QStringLiteral("sleepy.sh"));
    QFile file(script);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("#!/bin/sh\nsleep 5\n");
    file.close();
    QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner
                                              | QFile::ExeOwner));

    ProcessOwner owner;
    StopResult result = StopResult::TimedOut;
    {
        const ScopedSigtermProbe probe(markerFile.handle());
        // Exactly the call AsrAdapter makes when an abandoned request is
        // retired: terminate immediately, no grace. A SIGTERM that is sent
        // *after* exec gets 0 ms to work, so Killed is the expected, legal
        // escalation here — the assertion is the marker: no signal before exec.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        // Without help the fork→exec window is a few microseconds: by the time
        // start() returns, the child has normally exec'ed already, so the bug
        // hides and this test would pass even against the unfixed stop(). Hold
        // the child *between* fork and exec for 200 ms (well under kStartWaitMs)
        // — the modifier runs in the child, so only async-signal-safe work here.
        QProcess process;
        process.setChildProcessModifier([] {
            const timespec delay{0, 200 * 1000 * 1000};   // 200 ms
            ::nanosleep(&delay, nullptr);
        });
        process.start(script);
        // An adopted process is not deleted by the owner — the test owns it.
        owner.adopt(&process, true);
#else
        // Qt 5 has no child-process modifier; the window stays narrow, so this
        // variant proves "no signal on the failed-start path" only together
        // with stopBeforeExecReportsNotRunning().
        QVERIFY(owner.start(script, {}));
#endif
        result = owner.stop(0, 1000);
    }

    QVERIFY2(result != StopResult::TimedOut,
             "stop() returned TimedOut on a child that did start");
    QVERIFY(!owner.isRunning());
    QVERIFY2(QFileInfo(marker).size() == 0,
             "stop() sent SIGTERM to a child that had not exec'ed yet");
#endif
}

void ProcessOwnerTest::stopAfterStartStillTerminates()
{
#ifdef Q_OS_WIN
    QSKIP("POSIX fixture: uses `sleep` as the child");
#else
    const QString sleep = sleepPath();
    QVERIFY2(!sleep.isEmpty(), "no /bin/sleep on this machine");

    ProcessOwner owner;
    QVERIFY(owner.start(sleep, {QStringLiteral("30")}));
    QVERIFY2(waitStarted(owner.process()), "child never execed");

    // Regression: the normal path is unchanged — SIGTERM, bounded wait, no kill.
    QCOMPARE(owner.stop(3000, 1000), StopResult::Stopped);
    QVERIFY(!owner.isRunning());
    QCOMPARE(owner.processId(), 0);
#endif
}

void ProcessOwnerTest::ownedProcessIsTerminated()
{
#ifdef Q_OS_WIN
    QSKIP("POSIX fixture: the polite path needs SIGTERM, which a Windows console child does not handle");
#else
    const QString sleep = sleepPath();
    QVERIFY2(!sleep.isEmpty(), "no /bin/sleep on this machine");

    ProcessOwner owner;
    QVERIFY(owner.start(sleep, {QStringLiteral("30")}));
    QVERIFY(owner.isRunning());
    QVERIFY(owner.processId() != 0);

    // SIGTERM is enough for sleep(1): no kill, no 3 s grace period.
    QElapsedTimer timer;
    timer.start();
    QCOMPARE(owner.stop(3000, 1000), StopResult::Stopped);
    QVERIFY2(timer.elapsed() < 2500,
             qPrintable(QStringLiteral("polite stop took %1 ms").arg(timer.elapsed())));
    QVERIFY(!owner.isRunning());
#endif
}

void ProcessOwnerTest::foreignProcessIsNotKilled()
{
#ifdef Q_OS_WIN
    QSKIP("POSIX fixture: SIGTERM semantics are what is being asserted");
#else
    const QString sleep = sleepPath();
    QVERIFY(!sleep.isEmpty());

    // The test is the creator — exactly like a node that found a warm server.
    auto *foreign = new QProcess;
    foreign->start(sleep, {QStringLiteral("30")});
    QVERIFY(waitStarted(foreign));
    const qint64 pid = foreign->processId();

    ProcessOwner owner;
    owner.adopt(foreign, false);
    QVERIFY(!owner.isOwned());
    QVERIFY(owner.isRunning());
    QCOMPARE(owner.processId(), pid);

    // stop() must forget, not signal.
    QCOMPARE(owner.stop(), StopResult::NotRunning);
    QVERIFY(!owner.isRunning());
    QVERIFY2(foreign->state() != QProcess::NotRunning,
             "stop() killed a process it does not own");
    QCOMPARE(foreign->processId(), pid);

    foreign->kill();
    QVERIFY(foreign->waitForFinished(1000));
    delete foreign;
#endif
}

void ProcessOwnerTest::setOwnedFalseForbidsTheKill()
{
#ifdef Q_OS_WIN
    QSKIP("POSIX fixture: SIGTERM semantics are what is being asserted");
#else
    const QString sleep = sleepPath();
    QVERIFY(!sleep.isEmpty());

    auto *process = new QProcess;
    process->start(sleep, {QStringLiteral("30")});
    QVERIFY(waitStarted(process));

    ProcessOwner owner;
    owner.adopt(process, false);
    owner.setOwned(false); // explicit, although adopt() already did it
    QVERIFY(!owner.isOwned());

    QCOMPARE(owner.stop(), StopResult::NotRunning);
    QVERIFY2(process->state() != QProcess::NotRunning,
             "setOwned(false) must forbid both terminate() and kill()");

    process->kill();
    QVERIFY(process->waitForFinished(1000));
    delete process;
#endif
}

void ProcessOwnerTest::killFallbackOnStubbornProcess()
{
#ifdef Q_OS_WIN
    QSKIP("POSIX fixture: the stubborn child is a #!/bin/sh script");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString server = writeStubbornServer(dir);
    QVERIFY2(!server.isEmpty(), "could not write the fake server script");

    const QString ready = dir.filePath(QStringLiteral("ready.flag"));

    ProcessOwner owner;
    // workDir = the temp dir, so the script's `ready.flag` lands where we look.
    QVERIFY(owner.start(server, {}, dir.path()));
    // Wait for the trap handshake, not for a guessed delay: before `trap` runs
    // the shell still has the DEFAULT SIGTERM disposition and would die from a
    // signal that was not supposed to work.
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(ready), 5000);
    QVERIFY(owner.isRunning());

    // 300 ms of grace was already spent inside stop(); the script then needs
    // SIGKILL, which is the whole point of D2.
    QElapsedTimer timer;
    timer.start();
    QCOMPARE(owner.stop(300, 1000), StopResult::Killed);
    const qint64 elapsed = timer.elapsed();

    QVERIFY(!owner.isRunning());
    QVERIFY2(elapsed >= 300,
             qPrintable(QStringLiteral("stop() returned after %1 ms, before the grace period")
                            .arg(elapsed)));
    QVERIFY2(elapsed < 2000,
             qPrintable(QStringLiteral("stop() took %1 ms — kill() did not reap it").arg(elapsed)));
#endif
}

void ProcessOwnerTest::stopTwiceIsIdempotent()
{
#ifdef Q_OS_WIN
    QSKIP("POSIX fixture: uses `sleep` as the child");
#else
    const QString sleep = sleepPath();
    QVERIFY(!sleep.isEmpty());

    ProcessOwner owner;
    QVERIFY(owner.start(sleep, {QStringLiteral("30")}));
    QCOMPARE(owner.stop(3000, 1000), StopResult::Stopped);
    QCOMPARE(owner.stop(), StopResult::NotRunning);
    QVERIFY(!owner.isRunning());
    QCOMPARE(owner.processId(), 0);
#endif
}
