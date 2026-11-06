#pragma once

// SPDX-License-Identifier: MIT
//
// ProcessOwner — owns the lifetime of one child process (REQ-SW-PL-050).
//
// A child process and a thread are stopped differently: a thread is *joined*
// (the thread must return from run()), a process is *asked to exit* and may
// refuse (SIGTERM / WM_CLOSE is a request, SIGKILL / TerminateProcess is not).
// So this is a second primitive instead of a mode on ThreadOwner.
//
// Rules:
//
//   * a child that has not exec'ed yet is **not ours to signal**. While
//     `state() == Starting` the OS process exists (`processId()` is non-zero)
//     but exec() has not replaced the image yet, so the fork still carries the
//     *parent's* signal handlers. A SIGTERM there runs the parent's handler in
//     the child — in a QtTest process that is `QFATAL "Received signal 15"` plus
//     a core dump, not a polite shutdown. stop() therefore waits for `started`
//     or `errorOccurred` first (bounded by kStartWaitMs) and sends **no**
//     signal when the start failed. This is the owner's responsibility, never
//     the node's;
//   * `terminate()` → `waitForFinished(graceMs)` → `kill()` →
//     `waitForFinished(killWaitMs)`. The caller always gets an answer, in
//     bounded time; nothing blocks on a process that ignores the polite signal;
//   * `owned == false` means "someone else's process" (a reused server). stop()
//     then neither terminates nor kills it — it only forgets it. Without that
//     flag a node that *found* a warm whisper-server would kill a shared
//     resource on scene close;
//   * the process started here is deleted by its own `finished` → `deleteLater`,
//     so no signal outlives the object and no QPointer is left dangling.
//
// Threading contract: like QProcess itself, one instance belongs to the thread
// that created it (normally the GUI thread). `stop()` blocks that thread for at
// most `graceMs + killWaitMs`.

#include <QMetaObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace Daqster {

/// Owns one QProcess: start it, stop it in bounded time, never kill a foreign
/// one.
class ProcessOwner
{
public:
    /// How a stop attempt ended.
    enum class StopResult {
        NotRunning, ///< nothing owned (never started, failed to start, exited by itself, or foreign)
        Stopped,    ///< exited within the grace period after terminate()
        Killed,     ///< ignored terminate() and was killed
        TimedOut    ///< survived both, or never got past Starting (logged, left attached)
    };

    ProcessOwner() = default;

    /// Stops the process if it is still running (short deadlines: the normal
    /// path is IStoppable::stop(), this is the safety net).
    ~ProcessOwner() { stop(1000, 500); }

    ProcessOwner(const ProcessOwner &) = delete;
    ProcessOwner &operator=(const ProcessOwner &) = delete;
    ProcessOwner(ProcessOwner &&) = delete;
    ProcessOwner &operator=(ProcessOwner &&) = delete;

    /// Allow (true, default) or forbid (false) killing this process.
    ///
    /// Set to false for a process this owner did not spawn — a reused server, a
    /// daemon started by the user. stop() then disconnects and forgets instead
    /// of terminating it.
    void setOwned(bool owned) { m_owned = owned; }
    bool isOwned() const { return m_owned; }

    /// Start `program` with `args`.
    ///
    /// @param workDir Working directory for the child; empty = inherit.
    /// @return false when a process is already running, or when `program` is
    ///         empty. A failed *exec* (missing binary) is still a started
    ///         QProcess — it reports via `errorOccurred` and finishes; use
    ///         isRunning()/stop() for that.
    bool start(const QString &program, const QStringList &args, const QString &workDir = {});

    /// Ask the process to exit, then kill it if it does not, in bounded time.
    ///
    /// A process that is still `Starting` is waited for (see start(): a child
    /// that has not exec'ed is not signalled). Only a process that really
    /// started is terminated.
    ///
    /// @param graceMs    How long terminate() gets before kill().
    /// @param killWaitMs How long the process gets to die after kill().
    /// @return NotRunning when there is nothing to stop (idempotent) or the
    ///         start failed (no signal was sent),
    ///         Stopped on a clean exit, Killed when kill() was needed,
    ///         TimedOut when it survived both, or when it was still `Starting`
    ///         after the start deadline (left attached, logged).
    StopResult stop(int graceMs = 3000, int killWaitMs = 1000);

    /// True while the owned process is running.
    bool isRunning() const;

    /// The process this owner holds, or nullptr when it holds none.
    ///
    /// Borrowed: connect signals to it, read its streams, ask for its state —
    /// never delete it. The owner deletes a process it started (via its own
    /// `finished → deleteLater`); an adopted process keeps its creator's
    /// lifetime. The accessor exists because the owner is not a QObject and
    /// cannot be the receiver of `finished`/`started`/`readyRead` — every real
    /// user (whisper-server, piper) needs those signals.
    QProcess *process() const { return m_process.data(); }

    /// OS process id of the owned process, 0 when it is not running.
    qint64 processId() const;

    /// Take responsibility for a process created elsewhere.
    ///
    /// The object's lifetime stays with its creator (`owned == false` is the
    /// normal case for a reused server); the owner never deletes an adopted
    /// process.
    void adopt(QProcess *process, bool owned);

private:
    /// Drop every connection this owner made and delete the process it created.
    void forgetProcess();

    QPointer<QProcess> m_process;
    QMetaObject::Connection m_finishedConnection;
    bool m_owned = true;   ///< may we terminate/kill it?
    bool m_created = false; ///< did we `new` the QProcess (→ we must delete it)?
};

} // namespace Daqster
