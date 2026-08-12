#ifndef VIDEOPERFBADGE_H
#define VIDEOPERFBADGE_H

#include <QtGlobal>
#include <QString>

/**
 * @brief Formats the on-screen perf overlay badge (REQ-SW-PL-027).
 *
 * Pure, QtCore-only helper — no widgets, no QObject, no QtMultimedia — so it
 * can be unit-tested deterministically without wall-clock timing or a media
 * backend. The QTimer callback in VideoOutputNode feeds it the "video" domain
 * aggregates (raw nanoseconds) and the last-frame HW/SW markers.
 *
 * Output layout:
 *   `HW|SW · fmt=… · handle=… · fps=… · gap=…ms · present=…ms · total=…ms`
 *
 * @param gapNs      avg inter-frame gap (stage "source.frame_interval"), ns.
 *                   <= 0 (no samples / first frame) renders fps=0 and gap=0ms.
 * @param presentNs  avg present/blit time (stage "output.present"), ns.
 * @param totalNs    avg full setInData frame processing (stage "output.total"), ns.
 * @param handleType QVideoFrame::HandleType as int (0 = NoHandle → "SW").
 * @param pixelFormat QVideoFrameFormat::PixelFormat as int (Qt6 values).
 *
 * @return The formatted badge text.
 */
QString formatPerfBadge(qint64 gapNs, qint64 presentNs, qint64 totalNs,
                        int handleType, int pixelFormat);

#endif // VIDEOPERFBADGE_H
