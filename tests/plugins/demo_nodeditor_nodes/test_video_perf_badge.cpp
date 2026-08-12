#include <QtTest>

#include "VideoPerfBadge.h"
#include "test_video_perf_badge.h"

// REQ-SW-PL-027 overlay badge formatter unit tests. The formatter is pure
// (QtCore-only), so these tests are deterministic and do not depend on
// wall-clock timing or a multimedia backend.

void TestVideoPerfBadge::softwareHandleMapsToSw()
{
    // handleType == 0 (QVideoFrame::NoHandle) → software path.
    const QString text = formatPerfBadge(0, 0, 0, /*handleType=*/0, 0);
    QVERIFY(text.startsWith(QStringLiteral("SW ·")));
}

void TestVideoPerfBadge::hardwareHandleMapsToHw()
{
    // handleType == 1 (QVideoFrame::RhiTextureHandle) → hardware path.
    const QString text = formatPerfBadge(0, 0, 0, /*handleType=*/1, 0);
    QVERIFY(text.startsWith(QStringLiteral("HW ·")));
}

void TestVideoPerfBadge::fpsZeroEdgeCase()
{
    // gapNs == 0 (no samples / first frame) must render fps=0.0 and gap=0.00ms
    // without a division-by-zero.
    const QString text = formatPerfBadge(/*gapNs=*/0, 0, 0, 0, 0);
    QVERIFY(text.contains(QStringLiteral("fps=0.0")));
    QVERIFY(text.contains(QStringLiteral("gap=0.00ms")));
}

void TestVideoPerfBadge::nsToMsConversion()
{
    // 33_333_333 ns ≈ 30 fps and 33.33 ms; 1_000_000 ns = 1.00 ms.
    const QString text = formatPerfBadge(/*gapNs=*/33333333, /*presentNs=*/1000000,
                                         /*totalNs=*/5500000, 0, 0);
    QVERIFY(text.contains(QStringLiteral("fps=30.0")));
    QVERIFY(text.contains(QStringLiteral("gap=33.33ms")));
    QVERIFY(text.contains(QStringLiteral("present=1.00ms")));
    QVERIFY(text.contains(QStringLiteral("total=5.50ms")));
}

void TestVideoPerfBadge::enumToText()
{
    // Qt6 QVideoFrameFormat::PixelFormat: 18 = NV12, 13 = YUV420P.
    // Qt6 QVideoFrame::HandleType: 1 = RhiTextureHandle.
    const QString text = formatPerfBadge(0, 0, 0, /*handleType=*/1, /*pixelFormat=*/18);
    QVERIFY(text.contains(QStringLiteral("fmt=NV12")));
    QVERIFY(text.contains(QStringLiteral("handle=RhiTextureHandle")));

    const QString swYuv = formatPerfBadge(0, 0, 0, /*handleType=*/0, /*pixelFormat=*/13);
    QVERIFY(swYuv.contains(QStringLiteral("fmt=YUV420P")));
    QVERIFY(swYuv.contains(QStringLiteral("handle=NoHandle")));
}

void TestVideoPerfBadge::unknownEnumsFallBackToNumeric()
{
    // Out-of-range enums render their raw numeric value (never crash).
    const QString text = formatPerfBadge(0, 0, 0, /*handleType=*/7, /*pixelFormat=*/999);
    QVERIFY(text.contains(QStringLiteral("handle=7")));
    QVERIFY(text.contains(QStringLiteral("fmt=999")));
}

void TestVideoPerfBadge::noSamplesNegativeRendersZero()
{
    // Domain getters return -1 for stages with no samples; the formatter must
    // collapse those to 0 (not render "gap=-1ms").
    const QString text = formatPerfBadge(/*gapNs=*/-1, /*presentNs=*/-1,
                                         /*totalNs=*/-1, 0, 0);
    QVERIFY(text.contains(QStringLiteral("fps=0.0")));
    QVERIFY(text.contains(QStringLiteral("gap=0.00ms")));
    QVERIFY(text.contains(QStringLiteral("present=0.00ms")));
    QVERIFY(text.contains(QStringLiteral("total=0.00ms")));
}

void TestVideoPerfBadge::fullFormatLayout()
{
    // Lock the exact badge layout (U+00B7 separators, field order, precision).
    const QString text = formatPerfBadge(/*gapNs=*/33333333, /*presentNs=*/1000000,
                                         /*totalNs=*/5500000, /*handleType=*/1,
                                         /*pixelFormat=*/18);
    QCOMPARE(text,
             QStringLiteral("HW · fmt=NV12 · handle=RhiTextureHandle · fps=30.0 · gap=33.33ms · present=1.00ms · total=5.50ms"));
}

// No QTEST_GUILESS_MAIN here: the video test classes share one binary whose
// main lives in test_main.cpp.
