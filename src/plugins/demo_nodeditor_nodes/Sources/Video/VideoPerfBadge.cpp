#include "VideoPerfBadge.h"

namespace {

// ── HW/SW marker ────────────────────────────────────────────────────────────
//
// QVideoFrame::HandleType (Qt 6.x): NoHandle = 0, RhiTextureHandle = 1.
// The badge collapses this to a coarse HW/SW marker: NoHandle means the frame
// lives in system memory (software decode path), anything else is GPU/RHI
// (hardware decode path). REQ-SW-PL-027 AC 4.

bool isHardwareHandle(int handleType)
{
    return handleType != 0; // 0 == QVideoFrame::NoHandle
}

// ── enum → text ─────────────────────────────────────────────────────────────
//
// QVideoFrame::HandleType → display name (Qt 6.9.2 values). The overlay is
// Qt6-only, so the mapping is fixed to the Qt6 enumerator order.

QString handleTypeName(int handleType)
{
    switch (handleType) {
    case 0:  return QStringLiteral("NoHandle");
    case 1:  return QStringLiteral("RhiTextureHandle");
    default: return QString::number(handleType);
    }
}

// QVideoFrameFormat::PixelFormat → display name (Qt 6.9.2 values). Kept as a
// plain switch (QtCore-only) so the helper never depends on QtMultimedia.

QString pixelFormatName(int pixelFormat)
{
    switch (pixelFormat) {
    case -1: return QStringLiteral("Invalid"); // "no frame" (Qt5 QImage path)
    case 0:  return QStringLiteral("Invalid");
    case 1:  return QStringLiteral("ARGB8888");
    case 2:  return QStringLiteral("ARGB8888_Premultiplied");
    case 3:  return QStringLiteral("XRGB8888");
    case 4:  return QStringLiteral("BGRA8888");
    case 5:  return QStringLiteral("BGRA8888_Premultiplied");
    case 6:  return QStringLiteral("BGRX8888");
    case 7:  return QStringLiteral("ABGR8888");
    case 8:  return QStringLiteral("XBGR8888");
    case 9:  return QStringLiteral("RGBA8888");
    case 10: return QStringLiteral("RGBX8888");
    case 11: return QStringLiteral("AYUV");
    case 12: return QStringLiteral("AYUV_Premultiplied");
    case 13: return QStringLiteral("YUV420P");
    case 14: return QStringLiteral("YUV422P");
    case 15: return QStringLiteral("YV12");
    case 16: return QStringLiteral("UYVY");
    case 17: return QStringLiteral("YUYV");
    case 18: return QStringLiteral("NV12");
    case 19: return QStringLiteral("NV21");
    case 20: return QStringLiteral("IMC1");
    case 21: return QStringLiteral("IMC2");
    case 22: return QStringLiteral("IMC3");
    case 23: return QStringLiteral("IMC4");
    case 24: return QStringLiteral("Y8");
    case 25: return QStringLiteral("Y16");
    case 26: return QStringLiteral("P010");
    case 27: return QStringLiteral("P016");
    case 28: return QStringLiteral("SamplerExternalOES");
    case 29: return QStringLiteral("Jpeg");
    case 30: return QStringLiteral("SamplerRect");
    case 31: return QStringLiteral("YUV420P10");
    default: return QString::number(pixelFormat);
    }
}

} // namespace

QString formatPerfBadge(qint64 gapNs, qint64 presentNs, qint64 totalNs,
                        int handleType, int pixelFormat)
{
    // fps: proxy for the decode cadence. gapNs <= 0 (no samples recorded yet,
    // or the first frame whose gap is skipped) renders fps = 0.
    const double fps = gapNs > 0 ? 1e9 / static_cast<double>(gapNs) : 0.0;

    // ns → ms (negative/-1 "no samples" values collapse to 0.0).
    const double gapMs = gapNs > 0 ? static_cast<double>(gapNs) / 1e6 : 0.0;
    const double presentMs = presentNs > 0 ? static_cast<double>(presentNs) / 1e6 : 0.0;
    const double totalMs = totalNs > 0 ? static_cast<double>(totalNs) / 1e6 : 0.0;

    const QString hwSw = isHardwareHandle(handleType) ? QStringLiteral("HW")
                                                      : QStringLiteral("SW");

    return QStringLiteral("%1 · fmt=%2 · handle=%3 · fps=%4 · gap=%5ms · present=%6ms · total=%7ms")
        .arg(hwSw)
        .arg(pixelFormatName(pixelFormat))
        .arg(handleTypeName(handleType))
        .arg(fps, 0, 'f', 1)
        .arg(gapMs, 0, 'f', 2)
        .arg(presentMs, 0, 'f', 2)
        .arg(totalMs, 0, 'f', 2);
}

QString formatPerfLine(qint64 gapNs, qint64 presentNs, qint64 totalNs,
                       double cpuPercent, int handleType, int pixelFormat)
{
    // fps: proxy for the decode cadence. gapNs <= 0 (no samples recorded yet,
    // or the first frame whose gap is skipped) renders fps = 0.
    const double fps = gapNs > 0 ? 1e9 / static_cast<double>(gapNs) : 0.0;

    // ns → ms (negative/-1 "no samples" values collapse to 0.0).
    const double gapMs = gapNs > 0 ? static_cast<double>(gapNs) / 1e6 : 0.0;
    const double presentMs = presentNs > 0 ? static_cast<double>(presentNs) / 1e6 : 0.0;
    const double totalMs = totalNs > 0 ? static_cast<double>(totalNs) / 1e6 : 0.0;

    // cpu: negative values collapse to 0.0 (first sample returns 0.0 anyway).
    const double cpu = cpuPercent > 0.0 ? cpuPercent : 0.0;

    const QString hwSw = isHardwareHandle(handleType) ? QStringLiteral("HW")
                                                      : QStringLiteral("SW");

    return QStringLiteral("[PERF] video | %1 | fmt=%2 | handle=%3 | fps=%4 | gap=%5ms | present=%6ms | total=%7ms | cpu=%8%")
        .arg(hwSw)
        .arg(pixelFormatName(pixelFormat))
        .arg(handleTypeName(handleType))
        .arg(fps, 0, 'f', 0)
        .arg(gapMs, 0, 'f', 1)
        .arg(presentMs, 0, 'f', 1)
        .arg(totalMs, 0, 'f', 1)
        .arg(cpu, 0, 'f', 1);
}
