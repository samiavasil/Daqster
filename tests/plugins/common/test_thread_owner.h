#pragma once

#include <QtTest>

#include <QObject>
#include <QThread>

#include <atomic>

/// Worker used by ThreadOwnerTest: counts invocations and can block its own
/// thread's event loop, which is how a thread that "ignores quit()" is built
/// without inventing a custom QThread (ThreadOwner always runs an event loop).
class CountingWorker : public QObject
{
    Q_OBJECT

public:
    explicit CountingWorker(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    std::atomic<int> ticks{0};

    /// Raised as soon as blockFor() starts, so the test stops racing the queued
    /// invocation: after this is true the thread is provably busy.
    std::atomic<bool> enteredBlock{false};

public slots:
    void tick() { ticks.fetch_add(1, std::memory_order_relaxed); }

    /// Occupies the worker thread for `ms`. QThread::quit() only makes the
    /// event loop return once the current event handler finishes, so during
    /// this call the thread cannot be asked to stop — that is the timeout path.
    /// QThread::msleep() is a pthread cancellation point, so terminate() can
    /// still end the thread; that is exactly what stop() falls back to.
    void blockFor(int ms)
    {
        enteredBlock.store(true, std::memory_order_release);
        QThread::msleep(static_cast<unsigned long>(ms));
    }
};

/// Tests for ThreadOwner<Worker> (REQ-SW-PL-050): the latch-before-quit rule,
/// idempotent stop, the bounded join, and invoke()/alive() refusing work once
/// the owner is stopping.
class ThreadOwnerTest : public QObject
{
    Q_OBJECT

private slots:
    /// A never-started owner has nothing to stop and refuses work — this is the
    /// "close a scene that was never loaded" case.
    void stopBeforeStartIsNoOp();

    /// Control test: a live owner really does deliver the functor (so the
    /// no-op assertions below are not vacuously true).
    void invokeDeliversToWorker();

    /// Second stop() is NotRunning, not a second join or a crash.
    void stopTwiceIsIdempotent();

    /// After stop() the worker is gone and invoke() is a no-op that must not
    /// touch it — the functor is captured by value and would touch freed memory
    /// if it ever ran.
    void invokeAfterStopIsNoOp();

    /// A worker that cannot answer quit() in time is terminated; stop() reports
    /// TimedOut and still returns in bounded time instead of hanging forever.
    void stopTimesOutAndTerminates();
};
