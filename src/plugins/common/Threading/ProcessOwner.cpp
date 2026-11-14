#include "Threading/ProcessOwner.h"

#include "Threading/ThreadingLog.h"

#include <QLoggingCategory>

namespace Daqster {

namespace {
/// How long stop() waits for a `Starting` child to exec before giving up.
///
/// exec is a local operation — 500 ms is several orders of magnitude more than
/// a healthy one needs. The point is not to wait long, but to never signal a
/// process that has not exec'ed (see the invariant in ProcessOwner.h).
constexpr int kStartWaitMs = 500;
} // namespace

bool ProcessOwner::start(const QString &program, const QStringList &args, const QString &workDir)
{
    if (program.trimmed().isEmpty())
        return false;
    if (isRunning())
        return false;

    // Drop a previous process object that has already exited (its own
    // `finished` → `deleteLater` may not have been delivered yet).
    forgetProcess();

    auto *process = new QProcess();
    if (!workDir.isEmpty())
        process->setWorkingDirectory(workDir);

    m_process = process;
    m_created = true;
    m_owned = true;

    // A process we spawned deletes its own QProcess object when it finishes:
    // the owner is not a QObject, so nothing else could receive that signal.
    // QOverload: Qt5 also has the deprecated finished(int) overload.
    m_finishedConnection = QObject::connect(process,
                                            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                                            process, &QObject::deleteLater);

    process->start(program, args);
    return true;
}

ProcessOwner::StopResult ProcessOwner::stop(int graceMs, int killWaitMs)
{
    QProcess *process = m_process.data();
    if (!process || process->state() == QProcess::NotRunning) {
        forgetProcess();
        return StopResult::NotRunning;
    }

    if (!m_owned) {
        // Someone else's process (a reused server): never terminate, never
        // kill. Disconnect and forget — that is the whole point of the flag.
        qCWarning(lcThreading).noquote()
            << QStringLiteral("ProcessOwner: pid %1 is not owned — leaving it running")
                   .arg(process->processId());
        forgetProcess();
        return StopResult::NotRunning;
    }

    // 0. A child that has not exec'ed yet is not ours to signal: the fork
    //    still carries the parent's handlers (SIGTERM there = QFATAL in a
    //    QtTest parent, not a polite shutdown). waitForStarted() returns on
    //    `started` or on the failure signal, whichever comes first, so a
    //    FailedToStart is visible HERE — before any signal is sent.
    if (process->state() == QProcess::Starting)
        process->waitForStarted(kStartWaitMs);

    // Re-read through the QPointer: the wait above ran the event loop, and a
    // child that exited immediately schedules its own `finished → deleteLater`.
    process = m_process.data();
    if (!process || process->state() == QProcess::NotRunning) {
        // It either failed to exec or started and exited by itself: there was
        // never a process of ours to signal.
        forgetProcess();
        return StopResult::NotRunning;
    }
    if (process->state() == QProcess::Starting) {
        // exec did not complete within the deadline. Sending SIGTERM now is
        // exactly what the invariant forbids; report and leave it attached so a
        // later stop() can retry.
        qCWarning(lcThreading).noquote()
            << QStringLiteral("ProcessOwner: pid %1 still starting after %2 ms — "
                              "not signalling it")
                   .arg(process->processId())
                   .arg(kStartWaitMs);
        return StopResult::TimedOut;
    }

    const qint64 pid = process->processId();

    // 1. Polite request: SIGTERM on Unix, WM_CLOSE on Windows.
    process->terminate();
    if (process->waitForFinished(graceMs)) {
        forgetProcess();
        return StopResult::Stopped;
    }

    // 2. It refused. SIGKILL / TerminateProcess.
    qCWarning(lcThreading).noquote()
        << QStringLiteral("ProcessOwner: pid %1 ignored terminate() for %2 ms — killing")
               .arg(pid)
               .arg(graceMs);
    process->kill();
    if (process->waitForFinished(killWaitMs)) {
        forgetProcess();
        return StopResult::Killed;
    }

    // 3. Even killing did not reap it (uninterruptible sleep, stuck driver).
    //    Report instead of blocking the caller forever; the object stays so a
    //    later stop() can retry.
    qCWarning(lcThreading).noquote()
        << QStringLiteral("ProcessOwner: pid %1 still running %2 ms after kill() — giving up")
               .arg(pid)
               .arg(killWaitMs);
    return StopResult::TimedOut;
}

bool ProcessOwner::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

qint64 ProcessOwner::processId() const
{
    return m_process ? m_process->processId() : 0;
}

void ProcessOwner::adopt(QProcess *process, bool owned)
{
    forgetProcess();

    m_process = process;
    m_owned = owned;
    m_created = false;
    // No connections on purpose: the process' lifetime belongs to whoever
    // created it, so this owner must not delete it either.
}

void ProcessOwner::forgetProcess()
{
    if (m_finishedConnection)
        QObject::disconnect(m_finishedConnection);
    m_finishedConnection = {};

    QProcess *process = m_process.data();
    m_process.clear();

    if (m_created && process) {
        // Deleted by the event loop, like any other QObject whose owner is
        // gone: no signal can arrive at a half-destroyed owner afterwards.
        process->deleteLater();
    }
    m_created = false;
}

} // namespace Daqster
