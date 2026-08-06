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

} // namespace VideoCompat
