#include "test_process_owner.h"

#include <Threading/ProcessOwner.h>

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

namespace {

using Daqster::ProcessOwner;
using StopResult = ProcessOwner::StopResult;

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
