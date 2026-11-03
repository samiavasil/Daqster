#pragma once

// SPDX-License-Identifier: MIT
//
// ThreadOwner<Worker> — owns a QThread plus the QObject worker that lives on it
// (REQ-SW-PL-050).
//
// Why this exists: every capture/poller worker in the tree hand-rolled the same
// four steps (create the thread → moveToThread → connect finished→deleteLater →
// quit + wait) and each copy got one of them wrong — a `quit()` without a
// `wait()`, a `wait()` without a deadline, a worker pointer used after the
// thread had already deleted it. The rules are stated once, here:
//
//   * the stop latch is raised **before** `quit()`, so work offered during the
//     stop is refused instead of queued on a thread that is going away;
//   * `stop()` is idempotent — the second call is `NotRunning`;
//   * the join has a deadline. A thread that ignores `quit()` is terminated and
//     the caller is told (`TimedOut`); nothing waits forever;
//   * after a successful stop the worker pointer is null, so `alive()` and
//     `invoke()` cannot reach a half-destroyed worker.
//
// A thread and a process do not stop the same way — a thread is joined, a
// process reports an exit code — so `ProcessOwner` is a separate primitive
// (see Threading/ProcessOwner.h) rather than a mode flag on this one.
//
// Threading contract: `start()`/`stop()`/`alive()`/`invoke()` are for the
// thread that created the owner (normally the GUI thread); the **worker** runs
// on the owned thread. Widgets may be destroyed from any thread; a worker
// object is deleted either by its own `deleteLater` (delivered by the thread's
// event loop) or, if the loop stopped first, by `stop()` after the join.

#include <QLoggingCategory>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include "Threading/ThreadingLog.h"

namespace Daqster {

/// Owns the lifetime of one worker object and the QThread it runs on.
///
/// @tparam Worker QObject subclass living on the owned thread. It is created by
///                the factory passed to start() on the caller's thread, moved to
///                the new thread and deleted when the thread stops.
template <class Worker>
class ThreadOwner
{
    static_assert(std::is_base_of<QObject, Worker>::value,
                  "ThreadOwner<Worker> requires a QObject-derived worker: it is moved "
                  "with moveToThread(), deleted with deleteLater() and invoked through "
                  "QMetaObject.");

public:
    /// How a stop attempt ended.
    enum class StopResult {
        NotRunning, ///< nothing owned (never started, or already stopped)
        Stopped,    ///< quit() + join within the deadline
        TimedOut    ///< the deadline expired; terminate() was used (and logged)
    };

    ThreadOwner() = default;

    /// Stops the worker if it is still running (best effort, with deadlines).
    ~ThreadOwner() { stop(); }

    ThreadOwner(const ThreadOwner &) = delete;
    ThreadOwner &operator=(const ThreadOwner &) = delete;
    ThreadOwner(ThreadOwner &&) = delete;
    ThreadOwner &operator=(ThreadOwner &&) = delete;

    /// Create the worker, move it to a fresh thread and start that thread.
    ///
    /// @param makeWorker Callable returning `Worker*` (e.g. a lambda with
    ///                   `new MyWorker`). It runs on the calling thread and
    ///                   must return a **parentless** object — moveToThread()
    ///                   refuses objects that have a parent.
    /// @return false when the owner already has a thread/worker, or when the
    ///         factory returned nullptr. Nothing is started in that case.
    template <class Factory>
    bool start(Factory &&makeWorker)
    {
        if (m_thread || !m_worker.isNull())
            return false;

        Worker *worker = makeWorker();
        if (!worker)
            return false;

        auto *thread = new QThread();
        thread->setObjectName(QString::fromLatin1(worker->metaObject()->className()));

        worker->moveToThread(thread);
        // The worker is deleted by its own thread's event loop on the normal
        // path; stop() deletes it explicitly when the loop is already gone.
        QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);

        m_stopping.store(false, std::memory_order_release);
        m_worker = worker;
        m_thread = thread;
        thread->start();
        return true;
    }

    /// Ask the thread to finish and join it, with a hard deadline.
    ///
    /// Order matters: the latch is raised first, so `invoke()`/`alive()` refuse
    /// work for a worker that is about to be destroyed. If `quit()` is not
    /// honoured within `deadlineMs`, the thread is terminated; a thread that
    /// survives even that is left attached (destroying a running QThread aborts
    /// the process) and `TimedOut` is returned.
    ///
    /// `terminateOnTimeout == false` is for a worker whose thread must never be
    /// killed: PlutoSDR's stream loop sits inside `iio_buffer_refill()` on a
    /// libiio buffer that stop() is about to destroy, and terminating it there
    /// leaves the driver in an undefined state (REQ-SW-PL-050 D3). The deadline
    /// still applies — the caller gets `TimedOut` and the thread is left
    /// attached, exactly like a terminate() that did not reap it.
    ///
    /// @param deadlineMs Milliseconds to wait for the event loop to return.
    /// @param terminateMs Milliseconds to wait after terminate().
    /// @param terminateOnTimeout Whether terminate() may be used when the
    ///                           deadline expires. false = report and detach.
    /// @return NotRunning for an already-stopped owner (idempotent), Stopped on
    ///         a clean join, TimedOut when terminate() had to be used or when
    ///         the thread still would not die.
    StopResult stop(int deadlineMs = 3000, int terminateMs = 500,
                    bool terminateOnTimeout = true)
    {
        if (!m_thread) {
            return StopResult::NotRunning;
        }

        // 1. Latch BEFORE quit(): a task arriving during the stop must be
        //    refused, not queued on a thread that is being torn down.
        m_stopping.store(true, std::memory_order_release);

        QThread *thread = m_thread.data();
        StopResult result = StopResult::Stopped;

        // 2. Ask the event loop to return. Harmless for a thread that never ran
        //    an event loop or is still starting.
        thread->quit();

        if (!thread->wait(deadlineMs)) {
            if (!terminateOnTimeout) {
                qCWarning(lcThreading).noquote()
                    << QStringLiteral("ThreadOwner<%1>: worker did not stop within %2 ms — "
                                      "leaving it attached (terminate() disabled)")
                           .arg(QString::fromLatin1(Worker::staticMetaObject.className()))
                           .arg(deadlineMs);
                // m_thread stays set: the caller keeps the resource the thread
                // is still using, and a later stop() gets another chance.
                return StopResult::TimedOut;
            }
            qCWarning(lcThreading).noquote()
                << QStringLiteral("ThreadOwner<%1>: worker did not stop within %2 ms — terminating")
                       .arg(QString::fromLatin1(Worker::staticMetaObject.className()))
                       .arg(deadlineMs);
            thread->terminate();
            if (!thread->wait(terminateMs)) {
                qCWarning(lcThreading).noquote()
                    << QStringLiteral("ThreadOwner<%1>: thread still running %2 ms after "
                                      "terminate() — leaving it attached")
                           .arg(QString::fromLatin1(Worker::staticMetaObject.className()))
                           .arg(terminateMs);
                // m_thread stays set: destroying a running QThread aborts the
                // process, so a later stop() gets another chance instead.
                return StopResult::TimedOut;
            }
            result = StopResult::TimedOut;
        }

        // 3. The thread is joined. Drop the worker: its deleteLater() may never
        //    have been delivered (the event loop is gone), and QPointer tells us
        //    whether it still exists.
        m_thread.clear();
        if (Worker *worker = m_worker.data()) {
            delete worker;
        }
        m_worker.clear();
        delete thread;

        return result;
    }

    /// Give up ownership of the running thread WITHOUT stopping it.
    ///
    /// For a worker that must not be killed: PlutoSDR's loop blocks inside
    /// `iio_buffer_refill()` on a buffer stop() is about to destroy, and
    /// terminating the thread there leaves the libiio driver in an undefined
    /// state (REQ-SW-PL-050 D3). The thread keeps running, deletes its worker
    /// through the usual `finished → deleteLater`, and deletes itself, so the
    /// owner must not touch it — `~ThreadOwner()` becomes a no-op and a later
    /// `stop()` reports NotRunning.
    ///
    /// The caller takes responsibility for the resources the thread still uses:
    /// intentionally leak them, never destroy them under the live thread.
    void detach()
    {
        if (!m_thread)
            return;
        QThread *thread = m_thread.data();
        m_thread.clear();
        m_worker.clear();
        m_stopping.store(true, std::memory_order_release);
        // The owner is no longer around to delete the thread object; the
        // thread deletes itself when (if) its loop finally returns.
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }

    /// True while the owned thread is running (including "stopping, not yet
    /// joined").
    bool isRunning() const { return m_thread && m_thread->isRunning(); }
    /// The live worker, or nullptr when the owner is stopping/stopped.
    ///
    /// Goes through the same predicate as invoke() on purpose: after the latch
    /// is raised the worker must not be reachable, even for the few
    /// milliseconds before the thread is actually joined.
    QPointer<Worker> alive() const { return liveWorker(); }

    /// Queue `fn` on the worker's thread.
    ///
    /// @return false when the owner is stopping/stopped (the latch or a null
    ///         worker) or when QMetaObject refused the call. A false return is
    ///         the normal "the scene is closed" answer, not an error.
    template <class F>
    bool invoke(F &&fn)
    {
        Worker *worker = liveWorker();
        if (!worker)
            return false;
        return QMetaObject::invokeMethod(worker, std::forward<F>(fn), Qt::QueuedConnection);
    }

private:
    /// Single predicate behind alive() and invoke(): the latch is checked before
    /// the pointer, so a half-stopped owner never hands out its worker.
    Worker *liveWorker() const
    {
        if (m_stopping.load(std::memory_order_acquire))
            return nullptr;
        return m_worker.data();
    }

    QPointer<QThread> m_thread;       ///< owned; null when not running
    QPointer<Worker> m_worker;        ///< nulled by deleteLater()/delete
    std::atomic<bool> m_stopping{true}; ///< latched before quit() and by ~
};

} // namespace Daqster
