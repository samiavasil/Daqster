#include "test_video_effect_node.h"

#include "NodeDataTypes/VideoFrameData.h"
#include "Threading/ComputePool.h"
#include "VideoEffectNode.h"

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
// After a CPU result, the widget label must show the pool's per-key counters
// ("CPU <completed>/<submitted> · <skipped> skipped · <fps> fps out").
void VideoEffectNodeTest::cpuPath_metricLabel_refreshed()
{
    VideoEffectNode node;
    node.setEffect(QStringLiteral("blur"));

    node.setInData(makeSolidFrame(QSize(32, 32), Qt::green), 0);
    QTRY_VERIFY_WITH_TIMEOUT(node.outData(0) != nullptr, 5000);

    QVERIFY(node.m_metricLabel != nullptr);
    const QString text = node.m_metricLabel->text();
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

QTEST_MAIN(VideoEffectNodeTest)