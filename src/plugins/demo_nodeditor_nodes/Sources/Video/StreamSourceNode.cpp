#include "StreamSourceNode.h"

#include "AudioBufferToSampled.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "StreamUrlValidator.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

StreamSourceNode::StreamSourceNode()
    : m_videoFrameOut(std::make_shared<VideoFrameData>())
{
    buildWidget();

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
    if (m_player != nullptr)
        m_player->stop();
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject StreamSourceNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["url"] = m_urlEdit->text();
    return modelJson;
}

void StreamSourceNode::load(QJsonObject const &p)
{
    if (p.contains("url"))
        m_urlEdit->setText(p["url"].toString());
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

QWidget *StreamSourceNode::embeddedWidget()
{
    return m_widget;
}

void StreamSourceNode::buildWidget()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_urlEdit = new QLineEdit(m_widget);
    m_urlEdit->setPlaceholderText(tr("Stream URL (http:// or rtsp://)"));
    layout->addWidget(m_urlEdit);

    auto *controlRow = new QHBoxLayout();
    m_connectButton = new QPushButton(tr("Connect"), m_widget);
    m_statusLabel = new QLabel(tr("Disconnected"), m_widget);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    controlRow->addWidget(m_connectButton);
    controlRow->addWidget(m_statusLabel, 1);
    layout->addLayout(controlRow);

    connect(m_connectButton, &QPushButton::clicked,
            this, &StreamSourceNode::onConnectClicked);
}

void StreamSourceNode::onConnectClicked()
{
    if (m_isPlaying) {
        m_player->stop();
        return;
    }

    const QString urlString = m_urlEdit->text().trimmed();
    QString error;
    if (!StreamUrlValidator::isValidStreamUrl(urlString, &error)) {
        setStatus(error, false);
        return;
    }

    const QUrl url(urlString);
    VideoCompat::setMediaSource(m_player, url);
    m_player->play();
}

void StreamSourceNode::onFrameAvailable(const QVideoFrame &frame)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
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
        m_lastPixelFormat = static_cast<int>(frame.surfaceFormat().pixelFormat());
    }

    // Zero-copy transport: wrap the decoded frame (ref-count bump only) and
    // emit it downstream (REQ-SW-PL-020 AC 2). No QImage conversion in the
    // hot path — the single video-frame port carries the frame (REQ-SW-PL-032).
    {
        PERF_SCOPE("video", "source.wrap_emit");
        m_videoFrameOut->setFrame(VideoCompat::frameToFrame(frame));
        Q_EMIT dataUpdated(0);
    }
#else
    // Qt5 (NV12-direct, mirror of the Qt6 branch): the decoded probe frame is
    // wrapped as an OWNED copy (frameToOwnedFrame) and emitted on the
    // video-frame port (0). No QImage conversion in the hot path.
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
        m_videoFrameOut->setFrame(VideoCompat::frameToFrame(frame));
        if (m_videoFrameOut->hasFrame())
            Q_EMIT dataUpdated(0);
    }
#endif
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
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok ? QStringLiteral("color: green;")
                                    : QStringLiteral("color: gray;"));
}

void StreamSourceNode::updateConnectButton()
{
    m_connectButton->setText(m_isPlaying ? tr("Stop") : tr("Connect"));
}
