#include "test_thread_owner.h"

#include <Threading/ThreadOwner.h>

#include <QElapsedTimer>

#include <memory>

namespace {

using WorkerOwner = Daqster::ThreadOwner<CountingWorker>;
using StopResult = WorkerOwner::StopResult;

} // namespace

void ThreadOwnerTest::stopBeforeStartIsNoOp()
{
    WorkerOwner owner;

    QVERIFY(!owner.isRunning());
    QVERIFY(owner.alive().isNull());
    QCOMPARE(owner.stop(), StopResult::NotRunning);
    QVERIFY(!owner.invoke([] {}));
}

void ThreadOwnerTest::invokeDeliversToWorker()
{
    WorkerOwner owner;
    QVERIFY(owner.start([] { return new CountingWorker; }));

    QVERIFY(owner.isRunning());
    QVERIFY(!owner.alive().isNull());

    // Captured by value on purpose: the pointer is only read on the worker
    // thread, where it is valid until stop().
    CountingWorker *worker = owner.alive().data();
    QVERIFY(owner.invoke([worker] { worker->tick(); }));
    QTRY_COMPARE(worker->ticks.load(), 1);

    QCOMPARE(owner.stop(), StopResult::Stopped);
}

void ThreadOwnerTest::stopTwiceIsIdempotent()
{
    WorkerOwner owner;
    QVERIFY(owner.start([] { return new CountingWorker; }));

    QCOMPARE(owner.stop(), StopResult::Stopped);
    QVERIFY(!owner.isRunning());
    QVERIFY(owner.alive().isNull());

    // Idempotent: nothing left to join, nothing left to delete.
    QCOMPARE(owner.stop(), StopResult::NotRunning);
    QCOMPARE(owner.stop(), StopResult::NotRunning);
}

void ThreadOwnerTest::invokeAfterStopIsNoOp()
{
    WorkerOwner owner;
    QVERIFY(owner.start([] { return new CountingWorker; }));

    // The functor captures a *shared* counter, never the worker: after stop()
    // the worker is deleted, so a functor that touched it would be a
    // use-after-free rather than a failed assertion.
    auto hits = std::make_shared<int>(0);
    QVERIFY(owner.invoke([hits] { ++*hits; }));
    QTRY_COMPARE(*hits, 1);

    QCOMPARE(owner.stop(), StopResult::Stopped);
    QVERIFY(owner.alive().isNull());

    QVERIFY(!owner.invoke([hits] { ++*hits; }));
    QTest::qWait(50); // let the (dead) thread's queue turn, if any survived
    QCOMPARE(*hits, 1);
}

void ThreadOwnerTest::stopTimesOutAndTerminates()
{
    WorkerOwner owner;
    QVERIFY(owner.start([] { return new CountingWorker; }));

    CountingWorker *worker = owner.alive().data();
    QVERIFY(owner.invoke([worker] { worker->blockFor(5000); }));
    QTRY_VERIFY_WITH_TIMEOUT(worker->enteredBlock.load(), 2000);

    QElapsedTimer timer;
    timer.start();
    // 100 ms is far too short for quit() to be honoured: the worker still has
    // ~5 s left in blockFor(). stop() must terminate and report it.
    //
    // Expected side effect: Qt prints "Qt has caught an exception thrown from
    // an event handler" to stderr. That IS terminate() on Unix (pthread_cancel
    // unwinds the thread through Qt's event loop) and is exactly why the
    // primitive logs a warning before doing it.
    const StopResult result = owner.stop(100, 2000);
    const qint64 elapsed = timer.elapsed();

    QCOMPARE(result, StopResult::TimedOut);
    QVERIFY(!owner.isRunning());
    QVERIFY(owner.alive().isNull());
    // Bounded: the 5 s sleep did not become the test's waiting time.
    QVERIFY2(elapsed < 4000, qPrintable(QStringLiteral("stop() took %1 ms").arg(elapsed)));
    // And it is really over: a third call finds nothing.
    QCOMPARE(owner.stop(), StopResult::NotRunning);
}
