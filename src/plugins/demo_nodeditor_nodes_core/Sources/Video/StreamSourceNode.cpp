#include "StreamSourceNode.h"

#include "AudioBufferToSampled.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "StreamUrlValidator.h"

#include <QMediaPlayer>
#include <QUrl>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

StreamSourceNode::StreamSourceNode()
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
            this, &StreamSourceNode::onAudioBufferReceived);
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5: probe the player's decoded audio buffers.
    m_audioProbe = new QAudioProbe(this);
    if (m_audioProbe->setSource(m_player)) {
        connect(m_audioProbe, &QAudioProbe::audioBufferProbed,
                this, &StreamSourceNode::onAudioBufferReceived);
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
            this, &StreamSourceNode::onMediaStatusChanged);
    VideoCompat::connectPlayerError(
        m_player, this,
        [this](int error, const QString &errorString) {
            onPlayerError(static_cast<QMediaPlayer::Error>(error), errorString);
        });
}

StreamSourceNode::~StreamSourceNode()
{
    // Single shutdown path: stop() stops the media player (REQ-SW-PL-050).
    stop();
}

void StreamSourceNode::stop()
{
    // Idempotent: QMediaPlayer::stop() on an already-stopped player is a no-op.
    if (m_player != nullptr)
        m_player->stop();
    m_isPlaying = false;
}

/// Start streaming programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onConnectClicked() when not playing.
void StreamSourceNode::start()
{
    if (m_isPlaying)
        return;

    const QString urlString = m_url.trimmed();
    QString error;
    if (!StreamUrlValidator::isValidStreamUrl(urlString, &error)) {
        setStatus(error, false);
        return;
    }

    const QUrl url(urlString);
    VideoCompat::setMediaSource(m_player, url);
    m_player->play();
}

QJsonObject StreamSourceNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["url"] = m_url;
    return modelJson;
}

void StreamSourceNode::load(QJsonObject const &p)
{
    if (p.contains("url")) {
        m_url = p["url"].toString();
        Q_EMIT urlChanged(m_url);
    }
}

unsigned int StreamSourceNode::nPorts(PortType portType) const
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

NodeDataType StreamSourceNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    if (portIndex == 1)
        return SampledData().type();
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> StreamSourceNode::outData(PortIndex port)
{
    if (port == 1)
        return m_audioOut;
    return m_videoFrameOut;
}

void StreamSourceNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(data);
    Q_UNUSED(portIndex);
}

void StreamSourceNode::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 1)
        ++m_audioPortConnectionCount;
}

void StreamSourceNode::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 1 && m_audioPortConnectionCount > 0)
        --m_audioPortConnectionCount;
}

void StreamSourceNode::onConnectClicked()
{
    if (m_isPlaying) {
        m_player->stop();
        return;
    }

    const QString urlString = m_url.trimmed();
    QString error;
    if (!StreamUrlValidator::isValidStreamUrl(urlString, &error)) {
        setStatus(error, false);
        return;
    }

    const QUrl url(urlString);
    VideoCompat::setMediaSource(m_player, url);
    m_player->play();
}

void StreamSourceNode::onUrlChanged(const QString &url)
{
    m_url = url;
}

void StreamSourceNode::onFrameAvailable(const QVideoFrame &frame)
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

void StreamSourceNode::onAudioBufferReceived(const QAudioBuffer &buffer)
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

void StreamSourceNode::onPlaybackStateChanged(int state)
{
    m_isPlaying = (state == static_cast<int>(QMediaPlayer::PlayingState));
    updateConnectButton();
}

void StreamSourceNode::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::EndOfMedia:
    case QMediaPlayer::InvalidMedia:
        m_player->stop();
        m_isPlaying = false;
        updateConnectButton();
        setStatus(status == QMediaPlayer::InvalidMedia
                      ? tr("Invalid stream or connection lost")
                      : tr("Stream ended"),
                  false);
        break;
    case QMediaPlayer::LoadingMedia:
        setStatus(tr("Connecting..."), false);
        break;
    case QMediaPlayer::BufferedMedia:
    case QMediaPlayer::LoadedMedia:
        setStatus(tr("Streaming"), true);
        break;
    default:
        break;
    }
}

void StreamSourceNode::onPlayerError(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error);
    m_isPlaying = false;
    updateConnectButton();
    setStatus(tr("Stream error: %1").arg(errorString), false);
}

void StreamSourceNode::setStatus(const QString &text, bool ok)
{
    Q_EMIT statusChanged(text, ok);
}

void StreamSourceNode::updateConnectButton()
{
    Q_EMIT playingChanged(m_isPlaying);
}
