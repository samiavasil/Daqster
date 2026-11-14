#pragma once

#include <QtTest>

/// Tests for ProcessOwner (REQ-SW-PL-050): terminate → bounded wait → kill,
/// the "foreign process is never killed" flag, and idempotent stop.
///
/// The fixtures are real child processes (`sleep`), so the class is POSIX-only:
/// on Windows QProcess::terminate() posts WM_CLOSE to a window and a console
/// child has none, which makes the polite path untestable there. Each test
/// skips explicitly instead of failing for the wrong reason.
class ProcessOwnerTest : public QObject
{
    Q_OBJECT

private slots:
    /// Nothing started → nothing to stop, and no kill.
    void stopBeforeStartIsNoOp();

    /// start() refuses an empty program (a typo must not spawn anything).
    void startRejectsEmptyProgram();

    /// A program that never exec'ed (missing binary) is not signalled:
    /// stop() reports NotRunning instead of killing the fork.
    void stopBeforeExecReportsNotRunning();

    /// stop() while the child is still Starting waits for exec and does not
    /// signal the fork — the fork still carries the parent's handlers.
    void stopWhileStartingDoesNotSignalTheChild();

    /// Regression: a really started child is still terminated politely.
    void stopAfterStartStillTerminates();

    /// A child that exits on SIGTERM is reported as Stopped, not Killed.
    void ownedProcessIsTerminated();

    /// A reused/foreign process is never terminated: stop() forgets it.
    void foreignProcessIsNotKilled();

    /// setOwned(false) on a process we did start forbids the kill too.
    void setOwnedFalseForbidsTheKill();

    /// A child that ignores SIGTERM is killed; stop() reports Killed.
    void killFallbackOnStubbornProcess();

    /// Second stop() is NotRunning.
    void stopTwiceIsIdempotent();
};
