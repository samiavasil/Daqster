#pragma once

#include <QtGlobal>
#include <QUrl>
#include <QVariant>

#include <QtMultimedia/QCamera>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoFrame>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QCameraDevice>
#include <QtMultimedia/QMediaCaptureSession>
#include <QtMultimedia/QMediaDevices>
#include <QtMultimedia/QVideoSink>
#else
#include <QtMultimedia/QCameraInfo>
#include <QtMultimedia/QMediaContent>
#include <QtMultimedia/QVideoProbe>
#endif

#include <functional>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QAbstractVideoBuffer>

#include <QByteArray>
#include <QList>
#endif

/**
 * @brief Qt5/Qt6 multimedia compatibility shim for the video nodes.
 *
 * Mirrors the AudioCompat.h pattern: a namespace of type aliases and inline
 * helpers so callers stay version-agnostic. Abstracts:
 *   - frame capture: Qt5 QVideoProbe (videoFrameProbed) vs Qt6 QVideoSink
 *     (videoFrameChanged), both delivering a QVideoFrame that converts to QImage
 *   - camera enumeration: Qt5 QCameraInfo::availableCameras() vs Qt6
 *     QMediaDevices::videoInputs()
 *   - media source assignment: Qt5 QMediaPlayer::setMedia(QMediaContent) vs
 *     Qt6 QMediaPlayer::setSource(QUrl)
 *   - playback state signal: Qt5 stateChanged(QMediaPlayer::State) vs Qt6
 *     playbackStateChanged(QMediaPlayer::PlaybackState), transported as int
 *     (both enums share the same unscoped values, e.g. QMediaPlayer::PlayingState)
 */
namespace VideoCompat {

/// Map a frame for read-only access (version-agnostic: Qt6 uses
/// QVideoFrame::ReadOnly, Qt5 QAbstractVideoBuffer::ReadOnly).
inline bool mapForRead(QVideoFrame &frame)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return frame.map(QVideoFrame::ReadOnly);
#else
    return frame.map(QAbstractVideoBuffer::ReadOnly);
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

using CameraDevice = QCameraDevice;
using FrameProbe = QVideoSink;

inline QList<CameraDevice> availableCameras()
{
    return QMediaDevices::videoInputs();
}

inline CameraDevice defaultCamera()
{
    return QMediaDevices::defaultVideoInput();
}

inline bool isNull(const CameraDevice &device)
{
    return device.isNull();
}

inline QString cameraId(const CameraDevice &device)
{
    return device.id();
}

inline QString cameraDescription(const CameraDevice &device)
{
    return device.description();
}

inline bool attachFrameProbe(QCamera *camera, FrameProbe *probe)
{
    if (camera == nullptr || probe == nullptr)
        return false;
    // Qt 6.x routes camera video through a QMediaCaptureSession; own the
    // session as a child of the sink so its lifetime matches frame capture.
    auto *session = new QMediaCaptureSession(probe);
    session->setCamera(camera);
    session->setVideoOutput(probe);
    return true;
}

inline bool attachFrameProbe(QMediaPlayer *player, FrameProbe *probe)
{
    if (player == nullptr || probe == nullptr)
        return false;
    player->setVideoSink(probe);
    return true;
}

inline QImage frameToImage(const QVideoFrame &frame)
{
    return frame.toImage();
}

/// Identity: documents the zero-copy contract — the frame is transported
/// as-is, no conversion happens (REQ-SW-PL-020).
inline QVideoFrame frameToFrame(const QVideoFrame &frame)
{
    return frame;
}

/// Present a decoded frame on a QVideoSink (GPU path — no QImage copy).
inline void presentFrame(QVideoSink *sink, const QVideoFrame &frame)
{
    if (sink != nullptr)
        sink->setVideoFrame(frame);
}

inline void setMediaSource(QMediaPlayer *player, const QUrl &url)
{
    player->setSource(url);
}

inline int variantToInt(const QVariant &value, int defaultValue)
{
    bool ok = false;
    const int result = value.toInt(&ok);
    return ok ? result : defaultValue;
}

inline int playbackState(const QMediaPlayer *player)
{
    return static_cast<int>(player->playbackState());
}

inline QMetaObject::Connection connectPlayerError(
    QMediaPlayer *player,
    QObject *context,
    std::function<void(int, const QString &)> slot)
{
    return QObject::connect(player, &QMediaPlayer::errorOccurred, context,
                            [slot](QMediaPlayer::Error error, const QString &errorString)
                            { slot(static_cast<int>(error), errorString); });
}

inline QMetaObject::Connection connectCameraError(
    QCamera *camera,
    QObject *context,
    std::function<void(int, const QString &)> slot)
{
    return QObject::connect(camera, &QCamera::errorOccurred, context,
                            [slot](QCamera::Error error, const QString &errorString)
                            { slot(static_cast<int>(error), errorString); });
}

inline QMetaObject::Connection connectFrameProbed(
    FrameProbe *probe,
    QObject *context,
    std::function<void(const QVideoFrame &)> slot)
{
    return QObject::connect(probe, &QVideoSink::videoFrameChanged, context,
                            [slot](const QVideoFrame &frame) { slot(frame); });
}

inline QMetaObject::Connection connectPlaybackState(
    QMediaPlayer *player,
    QObject *context,
    std::function<void(int)> slot)
{
    return QObject::connect(player, &QMediaPlayer::playbackStateChanged, context,
                            [slot](QMediaPlayer::PlaybackState state)
                            { slot(static_cast<int>(state)); });
}

#else

using CameraDevice = QCameraInfo;
using FrameProbe = QVideoProbe;

inline QList<CameraDevice> availableCameras()
{
    return QCameraInfo::availableCameras();
}

inline CameraDevice defaultCamera()
{
    return QCameraInfo::defaultCamera();
}

inline bool isNull(const CameraDevice &device)
{
    return device.isNull();
}

inline QString cameraId(const CameraDevice &device)
{
    return device.deviceName();
}

inline QString cameraDescription(const CameraDevice &device)
{
    return device.description();
}

inline bool attachFrameProbe(QCamera *camera, FrameProbe *probe)
{
    if (camera == nullptr || probe == nullptr)
        return false;
    return probe->setSource(camera);
}

inline bool attachFrameProbe(QMediaPlayer *player, FrameProbe *probe)
{
    if (player == nullptr || probe == nullptr)
        return false;
    return probe->setSource(player);
}

inline QImage frameToImage(const QVideoFrame &frame)
{
    return frame.image();
}

// ── Owned planar video buffer (Qt5) ─────────────────────────────────────────
//
// Qt5 probe frames are NOT safe to hold beyond the signal (the backend
// recycles its buffers). This buffer owns a copy of the decoded planes
// (QByteArray storage) so a QVideoFrame built on top of it stays valid for as
// long as the VideoFrameData that carries it. Used by frameToOwnedFrame().
class OwnedPlanarVideoBuffer : public QAbstractPlanarVideoBuffer
{
public:
    OwnedPlanarVideoBuffer(const QList<QByteArray> &planes, const QList<int> &strides)
        : QAbstractPlanarVideoBuffer(QAbstractVideoBuffer::NoHandle)
        , m_planes(planes)
        , m_strides(strides)
    {
        for (int i = 0; i < m_planes.size(); ++i) {
            m_planeData[i] = reinterpret_cast<uchar *>(
                const_cast<char *>(m_planes.at(i).constData()));
        }
    }

    MapMode mapMode() const override
    {
        return QAbstractVideoBuffer::ReadOnly;
    }

    int map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) override
    {
        if (mode != QAbstractVideoBuffer::ReadOnly)
            return 0;

        const int planeCount = m_planes.size();
        for (int i = 0; i < planeCount; ++i) {
            bytesPerLine[i] = m_strides.at(i);
            data[i] = m_planeData[i];
        }
        if (numBytes != nullptr) {
            int total = 0;
            for (const QByteArray &plane : m_planes)
                total += plane.size();
            *numBytes = total;
        }
        return planeCount;
    }

    void unmap() override {}

private:
    QList<QByteArray> m_planes;
    QList<int> m_strides;
    uchar *m_planeData[4] = {nullptr, nullptr, nullptr, nullptr};
};

/// Qt5: build an OWNED copy of a probe frame so it can be transported through
/// the graph as VideoFrameData (probe buffers are recycled by the backend).
/// Only NV12 (Y + interleaved UV) and YUV420P (3 planes) are supported — the
/// formats the Qt5 decoder delivers for real video. Any other format returns
/// an invalid frame and the caller keeps the QImage path.
inline QVideoFrame frameToOwnedFrame(const QVideoFrame &frame)
{
    const QVideoFrame::PixelFormat pf = frame.pixelFormat();
    if (pf != QVideoFrame::Format_NV12 && pf != QVideoFrame::Format_YUV420P)
        return QVideoFrame();

    const int width = frame.width();
    const int height = frame.height();
    if (width <= 0 || height <= 0)
        return QVideoFrame();

    // QVideoFrame::map()/unmap() are non-const in Qt5; the implicit share keeps
    // this local a cheap copy of the probe frame.
    QVideoFrame mappable = frame;
    if (!mapForRead(mappable))
        return QVideoFrame();

    const int chromaH = (height + 1) / 2;
    const int planeCount = (pf == QVideoFrame::Format_NV12) ? 2 : 3;

    QList<QByteArray> planes;
    QList<int> strides;
    for (int i = 0; i < planeCount; ++i) {
        const int stride = mappable.bytesPerLine(i);
        const int planeHeight = (i == 0) ? height : chromaH;
        planes.append(QByteArray(
            reinterpret_cast<const char *>(mappable.bits(i)), stride * planeHeight));
        strides.append(stride);
    }
    mappable.unmap();

    return QVideoFrame(new OwnedPlanarVideoBuffer(planes, strides),
                       QSize(width, height), pf);
}

inline QVideoFrame frameToFrame(const QVideoFrame &frame)
{
    // Qt5: probe frames must not be held — build an owned copy.
    return frameToOwnedFrame(frame);
}

inline void setMediaSource(QMediaPlayer *player, const QUrl &url)
{
    player->setMedia(QMediaContent(url));
}

inline int variantToInt(const QVariant &value, int defaultValue)
{
    bool ok = false;
    const int result = value.toInt(&ok);
    return ok ? result : defaultValue;
}

inline int playbackState(const QMediaPlayer *player)
{
    return static_cast<int>(player->state());
}

inline QMetaObject::Connection connectPlayerError(
    QMediaPlayer *player,
    QObject *context,
    std::function<void(int, const QString &)> slot)
{
    // Qt 5.15 QMediaPlayer only emits error(Error) without an error string;
    // QOverload disambiguates the signal from the error() accessor.
    return QObject::connect(player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
                            context,
                            [slot](QMediaPlayer::Error error)
                            { slot(static_cast<int>(error), QString()); });
}

inline QMetaObject::Connection connectCameraError(
    QCamera *camera,
    QObject *context,
    std::function<void(int, const QString &)> slot)
{
    // Qt 5.15 QCamera::errorOccurred(Error) carries no error string.
    return QObject::connect(camera, &QCamera::errorOccurred, context,
                            [slot](QCamera::Error error)
                            { slot(static_cast<int>(error), QString()); });
}

inline QMetaObject::Connection connectFrameProbed(
    FrameProbe *probe,
    QObject *context,
    std::function<void(const QVideoFrame &)> slot)
{
    return QObject::connect(probe, &QVideoProbe::videoFrameProbed, context,
                            [slot](const QVideoFrame &frame) { slot(frame); });
}

inline QMetaObject::Connection connectPlaybackState(
    QMediaPlayer *player,
    QObject *context,
    std::function<void(int)> slot)
{
    return QObject::connect(player, &QMediaPlayer::stateChanged, context,
                            [slot](QMediaPlayer::State state)
                            { slot(static_cast<int>(state)); });
}

#endif

/// Normalized pixel-format integer for the [PERF] markers / badge. Returns the
/// Qt6 QVideoFrameFormat::PixelFormat numbering on BOTH Qt versions so the
/// display names in VideoPerfBadge.cpp stay consistent (Qt5's own
/// QVideoFrame::PixelFormat numbering differs).
inline int pixelFormatInt(const QVideoFrame &frame)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return static_cast<int>(frame.surfaceFormat().pixelFormat());
#else
    switch (frame.pixelFormat()) {
    case QVideoFrame::Format_NV12:
        return 18; // Qt6 QVideoFrameFormat::Format_NV12
    case QVideoFrame::Format_YUV420P:
        return 13; // Qt6 QVideoFrameFormat::Format_YUV420P
    default:
        return -1;
    }
#endif
}

} // namespace VideoCompat
