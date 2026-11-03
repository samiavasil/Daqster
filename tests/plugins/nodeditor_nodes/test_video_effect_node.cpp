#include "test_video_effect_node.h"

#include "NodeDataTypes/VideoFrameData.h"
#include "Threading/ComputePool.h"
#include "Processing/Video/VideoEffectNode.h"

#include <QImage>
#include <QLabel>
#include <QVideoFrame>

using namespace QtNodes;
using Daqster::ComputePool;

namespace {

// A solid-color ARGB32 frame wrapped in VideoFrameData (CPU-resident).
std::shared_ptr<VideoFrameData> makeSolidFrame(const QSize &size, const QColor &color)
{
    QImage img(size, QImage::Format_ARGB32);
    img.fill(color);
    return std::make_shared<VideoFrameData>(QVideoFrame(img));
}

} // namespace

// ── CpuOnly effect (blur) — async CPU result ─────────────────────────────────
//
// Blur is registered as CpuOnly, so it ALWAYS runs on the CPU regardless of GL
// availability. setInData() submits to the shared ComputePool; the result
// arrives via the queued onCpuResult() slot. QTRY_VERIFY processes events until
// the output frame appears.
void VideoEffectNodeTest::cpuPath_blurEffect_asyncResult()
{
    VideoEffectNode node;
    node.setEffect(QStringLiteral("blur"));

    const QSize size(64, 48);
    node.setInData(makeSolidFrame(size, Qt::red), 0);

    // The pool runs on a real thread; the queued result needs event processing.
    QTRY_VERIFY_WITH_TIMEOUT(node.outData(0) != nullptr, 5000);

    auto output = std::dynamic_pointer_cast<VideoFrameData>(node.outData(0));
    QVERIFY(output != nullptr);
    QVERIFY(output->hasFrame());
    QCOMPARE(output->frame().size(), size);

    // The pool's per-key counters must reflect the submission + completion.
    QVERIFY(ComputePool::instance().submitted(node.m_poolKey) >= 1);
    QVERIFY(ComputePool::instance().completed(node.m_poolKey) >= 1);
}

// ── GpuOrCpu effect falls back to CPU without hardware GL ────────────────────
//
// Brightness is GpuOrCpu; in the headless test environment
// VideoGLContextManager::hasHardwareGL() is false, so the node must take the
// CPU path — same async contract as the CpuOnly effects.
void VideoEffectNodeTest::cpuPath_gpuOrCpu_fallsBackToCpu()
{
    VideoEffectNode node;
    node.setEffect(QStringLiteral("brightness"));

    const QSize size(32, 32);
    node.setInData(makeSolidFrame(size, Qt::blue), 0);

    QTRY_VERIFY_WITH_TIMEOUT(node.outData(0) != nullptr, 5000);

    auto output = std::dynamic_pointer_cast<VideoFrameData>(node.outData(0));
    QVERIFY(output != nullptr);
    QVERIFY(output->hasFrame());
    QCOMPARE(output->frame().size(), size);
}

// ── Metric label refresh ─────────────────────────────────────────────────────
//
// After a CPU result, the metric accessor must reflect the pool's per-key
// counters ("CPU <completed>/<submitted> · <skipped> skipped · <fps> fps out").
void VideoEffectNodeTest::cpuPath_metricLabel_refreshed()
{
    VideoEffectNode node;
    node.setEffect(QStringLiteral("blur"));

    node.setInData(makeSolidFrame(QSize(32, 32), Qt::green), 0);
    QTRY_VERIFY_WITH_TIMEOUT(node.outData(0) != nullptr, 5000);

    const QString text = node.metricLabelText();
    QVERIFY2(!text.isEmpty(), qPrintable(text));
    QVERIFY2(text.startsWith(QStringLiteral("CPU ")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("skipped")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("fps out")), qPrintable(text));
}

// ── m_totalFrames counter ────────────────────────────────────────────────────
//
// Every valid setInData() call increments m_totalFrames — including the
// reprocessCurrentFrame() re-submission path.
void VideoEffectNodeTest::cpuPath_totalFrames_incremented()
{
    VideoEffectNode node;
    node.setEffect(QStringLiteral("blur"));

    QCOMPARE(node.m_totalFrames, quint64(0));

    node.setInData(makeSolidFrame(QSize(32, 32), Qt::yellow), 0);
    QTRY_VERIFY_WITH_TIMEOUT(node.outData(0) != nullptr, 5000);
    QCOMPARE(node.m_totalFrames, quint64(1));

    // A second frame bumps the counter again.
    node.setInData(makeSolidFrame(QSize(32, 32), Qt::cyan), 0);
    QTRY_VERIFY_WITH_TIMEOUT(node.outData(0) != nullptr, 5000);
    QCOMPARE(node.m_totalFrames, quint64(2));
}

// ── Cancellation token (REQ-SW-PL-050 D1) ────────────────────────────────────
//
// The CPU task used to guard itself with `if (m_shuttingDown.load()) return;` —
// a read through `this`, which is exactly what may already be freed when the
// task is still running 500 ms after stop(). The guard is now
// Daqster::isCancelled(token) with the token captured BY VALUE, so this asserts
// the handle the worker keeps still answers after the node is gone.
void VideoEffectNodeTest::cancelledTaskGuardSurvivesTheNode()
{
    auto *node = new VideoEffectNode;
    const Daqster::CancelTokenPtr token = node->computeToken();
    QVERIFY(token != nullptr);
    QVERIFY2(!Daqster::isCancelled(token), "a live node must let its tasks run");

    node->stop();
    QVERIFY2(Daqster::isCancelled(token),
             "stop() must cancel the token BEFORE draining the pool, so a running "
             "task bails out instead of reading the node");

    delete node;

    // Whatever the freed node's bytes become, the task's own copy still says
    // "cancelled" — that is the whole point of capturing it by value.
    QVERIFY(Daqster::isCancelled(token));
}

QTEST_MAIN(VideoEffectNodeTest)