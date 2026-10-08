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

// ── formatPerfLine (console report, REQ-SW-PL-027) ──────────────────────────
//
// The single-line console report is the copy-paste-able form of the badge: a
// stable "[PERF] video" prefix, '|' separators, integer fps, 1-decimal ms, and
// a trailing cpu=…% field. The formatter is pure (QtCore-only), so these tests
// are deterministic.

void TestVideoPerfBadge::consoleLineCpuFormatting()
{
    // cpu is rendered with one decimal and a '%' suffix.
    const QString line = formatPerfLine(/*gapNs=*/33333333, /*presentNs=*/1800000,
                                        /*totalNs=*/14500000, /*cpu=*/43.1,
                                        /*handleType=*/0, /*pixelFormat=*/18);
    QVERIFY(line.contains(QStringLiteral("cpu=43.1%")));

    // Zero (first ProcessCpu sample) and negative CPU collapse to 0.0%.
    QVERIFY(formatPerfLine(0, 0, 0, /*cpu=*/0.0, 0, 0).contains(QStringLiteral("cpu=0.0%")));
    QVERIFY(formatPerfLine(0, 0, 0, /*cpu=*/-1.0, 0, 0).contains(QStringLiteral("cpu=0.0%")));
}

void TestVideoPerfBadge::consoleLineFpsZeroEdge()
{
    // gapNs == 0 (no samples / first frame) must render fps=0 and gap=0.0ms
    // without a division-by-zero.
    const QString line = formatPerfLine(/*gapNs=*/0, 0, 0, 0.0, 0, 0);
    QVERIFY(line.contains(QStringLiteral("fps=0 |")));
    QVERIFY(line.contains(QStringLiteral("gap=0.0ms")));
}

void TestVideoPerfBadge::consoleLineNegativeCollapse()
{
    // Domain getters return -1 for stages with no samples; the formatter must
    // collapse those (and a negative cpu) to 0, never render "gap=-1ms".
    const QString line = formatPerfLine(/*gapNs=*/-1, /*presentNs=*/-1,
                                        /*totalNs=*/-1, /*cpu=*/-1.0, 0, 0);
    QVERIFY(line.contains(QStringLiteral("fps=0 |")));
    QVERIFY(line.contains(QStringLiteral("gap=0.0ms")));
    QVERIFY(line.contains(QStringLiteral("present=0.0ms")));
    QVERIFY(line.contains(QStringLiteral("total=0.0ms")));
    QVERIFY(line.contains(QStringLiteral("cpu=0.0%")));
}

void TestVideoPerfBadge::consoleLineFullLayout()
{
    // Lock the exact console layout ('|' separators, integer fps, 1-decimal ms,
    // trailing cpu=…%).
    const QString line = formatPerfLine(/*gapNs=*/33333333, /*presentNs=*/1800000,
                                        /*totalNs=*/14500000, /*cpu=*/43.1,
                                        /*handleType=*/0, /*pixelFormat=*/18);
    QCOMPARE(line,
             QStringLiteral("[PERF] video | SW | fmt=NV12 | handle=NoHandle | fps=30 | gap=33.3ms | present=1.8ms | total=14.5ms | cpu=43.1%"));
}

void TestVideoPerfBadge::consoleLineStablePrefix()
{
    // Stable "[PERF] video" prefix for grep / copy-paste (REQ-SW-PL-027).
    QVERIFY(formatPerfLine(0, 0, 0, 0.0, 0, 0).startsWith(QStringLiteral("[PERF] video |")));
    // HW/SW marker maps identically to the badge.
    QVERIFY(formatPerfLine(0, 0, 0, 0.0, /*handleType=*/1, 0).contains(QStringLiteral("| HW |")));
}

// No QTEST_GUILESS_MAIN here: the video test classes share one binary whose
// main lives in test_main.cpp.
