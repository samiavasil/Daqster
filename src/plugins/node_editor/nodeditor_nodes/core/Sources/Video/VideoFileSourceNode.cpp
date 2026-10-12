#include "VideoFileSourceNode.h"

#include "AudioBufferToSampled.h"
#include "NodeDataTypes/VideoFrameData.h"

#include <QMediaPlayer>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoFileSourceNode::VideoFileSourceNode()
    : m_videoFrameOut(std::make_shared<VideoFrameData>())
{
    m_player = new QMediaPlayer(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6: audio is only routed to a sink when an explicit QAudioOutput is
    // set on the player — otherwise playback is silent (REQ-SW-PL-022 AC 1).
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Qt6 (6.8+): receive decoded audio buffers for the SampledData port.
    m_audioBufferOutput = new QAudioBufferOutput(this);
    m_player->setAudioBufferOutput(m_audioBufferOutput);
    connect(m_audioBufferOutput, &QAudioBufferOutput::audioBufferReceived,
            this, &VideoFileSourceNode::onAudioBufferReceived);
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5: probe the player's decoded audio buffers.
    m_audioProbe = new QAudioProbe(this);
    if (m_audioProbe->setSource(m_player)) {
        connect(m_audioProbe, &QAudioProbe::audioBufferProbed,
                this, &VideoFileSourceNode::onAudioBufferReceived);
    }
#endif
    m_frameProbe = new VideoCompat::FrameProbe(this);

    if (!VideoCompat::attachFrameProbe(m_player, m_frameProbe))
        setStatus(tr("Frame capture unavailable"), false);

    VideoCompat::connectFrameProbed(
        m_frameProbe, this,
        [this](const QVideoFrame &frame) { onFrameAvailable(frame); });

    VideoCompat::connectPlaybackState(
        m_player, this,
        [this](int state) { onPlaybackStateChanged(state); });

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &VideoFileSourceNode::onMediaStatusChanged);
    VideoCompat::connectPlayerError(
        m_player, this,
        [this](int error, const QString &errorString) {
            onPlayerError(static_cast<QMediaPlayer::Error>(error), errorString);
        });

    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        Q_EMIT positionChanged(pos, m_player->duration());
    });
}

VideoFileSourceNode::~VideoFileSourceNode()
{
    // Single shutdown path: stop() stops the media player (REQ-SW-PL-050).
    stop();
}

void VideoFileSourceNode::stop()
{
    // Idempotent: QMediaPlayer::stop() on an already-stopped player is a no-op.
    if (m_player != nullptr)
        m_player->stop();
    m_isPlaying = false;
}

/// Start playback programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onPlayPauseClicked() when not playing.
void VideoFileSourceNode::start()
{
    if (m_isPlaying)
        return;

    const QString filePath = currentFilePath();
    if (filePath.isEmpty()) {
        setStatus(tr("Choose a video file first"), false);
        return;
    }

    // Re-apply the media source when the file changed or playback ended,
    // so play() always starts from the beginning.
    if (filePath != m_loadedPath) {
        m_player->stop();
        VideoCompat::setMediaSource(m_player, QUrl::fromLocalFile(filePath));
        m_loadedPath = filePath;
    }

    m_player->play();
    updateTransportEnabled();
}

QJsonObject VideoFileSourceNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["filePath"] = m_filePath;
    return modelJson;
}

void VideoFileSourceNode::load(QJsonObject const &p)
{
    if (p.contains("filePath")) {
        m_filePath = p["filePath"].toString();
        Q_EMIT filePathChanged(m_filePath);
    }
}

unsigned int VideoFileSourceNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::Out:
        // Port 0: "video-frame" (zero-copy), port 1: "sample" (audio,
        // no gap — REQ-SW-PL-022).
        return 2;
    default:
        return 0;
    }
}

NodeDataType VideoFileSourceNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    if (portIndex == 1)
        return SampledData().type();
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> VideoFileSourceNode::outData(PortIndex port)
{
    if (port == 1)
        return m_audioOut;
    return m_videoFrameOut;
}

void VideoFileSourceNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(data);
    Q_UNUSED(portIndex);
}

void VideoFileSourceNode::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 1)
        ++m_audioPortConnectionCount;
}

void VideoFileSourceNode::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 1 && m_audioPortConnectionCount > 0)
        --m_audioPortConnectionCount;
}

void VideoFileSourceNode::onBrowseClicked()
{
    // The GUI plugin owns the widget tree (REQ-SW-PL-051), so the file dialog
    // lives there — see VideoFileSourceWidget. The model only receives the
    // chosen path through onFilePathChanged().
}

void VideoFileSourceNode::onFilePathChanged(const QString &filePath)
{
    m_filePath = filePath;
}

void VideoFileSourceNode::onPlayPauseClicked()
{
    if (m_isPlaying) {
        m_player->pause();
        return;
    }

    const QString filePath = currentFilePath();
    if (filePath.isEmpty()) {
        setStatus(tr("Choose a video file first"), false);
        return;
    }

    // Re-apply the media source when the file changed or playback ended,
    // so play() always starts from the beginning.
    if (filePath != m_loadedPath) {
        m_player->stop();
        VideoCompat::setMediaSource(m_player, QUrl::fromLocalFile(filePath));
        m_loadedPath = filePath;
    }

    m_player->play();
    updateTransportEnabled();
}

void VideoFileSourceNode::onStopClicked()
{
    m_player->stop();
    m_loadedPath.clear();
    m_isPlaying = false;
    updatePlayButton();  // set to "Play"
    setStatus(tr("Stopped"), false);  // gray status
    updateTransportEnabled();
}

void VideoFileSourceNode::onSeekBackClicked()
{
    const qint64 pos = m_player->position();
    m_player->setPosition(qMax(qint64(0), pos - 5000)); // -5 seconds
}

void VideoFileSourceNode::onSeekForwardClicked()
{
    const qint64 pos = m_player->position();
    const qint64 dur = m_player->duration();
    if (dur > 0)
        m_player->setPosition(qMin(dur, pos + 5000)); // +5 seconds
}

void VideoFileSourceNode::onFrameAvailable(const QVideoFrame &frame)
{
    // Runtime profiling (REQ-SW-PL-027): inter-frame gap is a proxy for the
    // decode cadence (the backend decodes before onFrameAvailable) and the
    // HW/SW markers tag the actual frame path. Everything is a no-op while the
    // "video" domain is disabled — the PERF_ENABLED guard avoids the clock read.
    if (PERF_ENABLED("video")) {
        const std::int64_t gapNs = m_perfWatch.mark();
        if (!m_perfFirstFrame && gapNs > 0)
            Daqster::Perf::Domain::get("video").record("source.frame_interval", gapNs);
        m_perfFirstFrame = false;

        m_lastHandleType = static_cast<int>(frame.handleType());
        m_lastPixelFormat = VideoCompat::pixelFormatInt(frame);
    }

    {
        PERF_SCOPE("video", "source.wrap_emit");
        // Fresh VideoFrameData per frame — never mutate the shared object a
        // consumer may still hold (frame aliasing). VideoCompat::frameToFrame()
        // dispatches internally: Qt6 = identity (ref-count bump), Qt5 = owned
        // copy (frameToOwnedFrame, may fail for unsupported formats).
        m_videoFrameOut = std::make_shared<VideoFrameData>(VideoCompat::frameToFrame(frame));
        if (m_videoFrameOut->hasFrame())
            Q_EMIT dataUpdated(0);
    }
}

void VideoFileSourceNode::onAudioBufferReceived(const QAudioBuffer &buffer)
{
    // Invalid/empty buffer (end-of-stream flush): ignore — no EOS type is
    // emitted (REQ-SW-PL-022 §4).
    if (!buffer.isValid() || buffer.byteCount() <= 0)
        return;

    // Wrap only — no sample conversion in the handler (REQ-SW-PL-022 §4).
    // QByteArray copy of the ~7 KB block is acceptable (<1 µs).
    m_audioOut = AudioBufferToSampled::wrapBuffer(buffer, name(), 10.0);
    if (!m_audioOut)
        return;

    // Emit only while a downstream consumer is connected (connection-count
    // model, like the video-frame port).
    if (m_audioPortConnectionCount <= 0)
        return;

    Q_EMIT dataUpdated(audioPortIndex());
}

void VideoFileSourceNode::onPlaybackStateChanged(int state)
{
    m_isPlaying = (state == static_cast<int>(QMediaPlayer::PlayingState));
    updatePlayButton();
}

void VideoFileSourceNode::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::EndOfMedia:
        m_player->stop();
        m_loadedPath.clear(); // force media re-set on the next play click
        m_isPlaying = false;
        updatePlayButton();
        setStatus(tr("End of video"), false);
        updateTransportEnabled();
        break;
    case QMediaPlayer::LoadingMedia:
        setStatus(tr("Loading..."), false);
        break;
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        setStatus(tr("Ready"), true);
        break;
    case QMediaPlayer::InvalidMedia:
        setStatus(tr("Invalid or unsupported video file"), false);
        break;
    default:
        break;
    }
}

void VideoFileSourceNode::onPlayerError(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error);
    m_isPlaying = false;
    updatePlayButton();
    setStatus(tr("Player error: %1").arg(errorString), false);
}

void VideoFileSourceNode::setStatus(const QString &text, bool ok)
{
    Q_EMIT statusChanged(text, ok);
}

QString VideoFileSourceNode::currentFilePath() const
{
    return m_filePath.trimmed();
}

void VideoFileSourceNode::updatePlayButton()
{
    Q_EMIT playingChanged(m_isPlaying);
}

void VideoFileSourceNode::updateTransportEnabled()
{
    // Stop / seek are only meaningful once a media source has been set.
    const bool enabled = !m_loadedPath.isEmpty();
    if (enabled == m_transportEnabled)
        return;
    m_transportEnabled = enabled;
    Q_EMIT transportEnabledChanged(enabled);
}
