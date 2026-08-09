#include "StreamSourceNode.h"

#include "NodeDataTypes/ImageData.h"
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
    : m_output(std::make_shared<ImageData>())
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    , m_videoFrameOut(std::make_shared<VideoFrameData>())
#endif
{
    buildWidget();

    m_player = new QMediaPlayer(this);
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // Port 0: "video-frame" (zero-copy), port 1: "image" (converted on demand).
        return 2;
#else
        return 1;
#endif
    default:
        return 0;
    }
}

NodeDataType StreamSourceNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (portIndex == 1)
        return ImageData().type();
    return VideoFrameData().type();
#else
    Q_UNUSED(portIndex);
    return ImageData().type();
#endif
}

std::shared_ptr<NodeData> StreamSourceNode::outData(PortIndex port)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (port == 1)
        return m_output;
    return m_videoFrameOut;
#else
    Q_UNUSED(port);
    return m_output;
#endif
}

void StreamSourceNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(data);
    Q_UNUSED(portIndex);
}

void StreamSourceNode::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (conId.outPortIndex == 1)
        ++m_imagePortConnectionCount;
#else
    Q_UNUSED(conId);
#endif
}

void StreamSourceNode::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (conId.outPortIndex == 1 && m_imagePortConnectionCount > 0)
        --m_imagePortConnectionCount;
#else
    Q_UNUSED(conId);
#endif
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
    // Zero-copy transport: wrap the decoded frame (ref-count bump only) and
    // emit it downstream (REQ-SW-PL-020 AC 2). No QImage conversion in the
    // hot path — the "image" port is converted only when a processing
    // consumer is connected.
    m_videoFrameOut->setFrame(VideoCompat::frameToFrame(frame));
    Q_EMIT dataUpdated(0);

    // TEMPORARY Qt6 diagnostics — remove after green-screen diagnosis.
    static int s_diagFrameCount = 0;
    static bool s_diagDumped = false;
    if (++s_diagFrameCount <= 10) {
        qDebug() << "VideoDiag" << name() << "frame" << s_diagFrameCount
                 << "valid" << frame.isValid()
                 << "fmt" << frame.surfaceFormat().pixelFormat()
                 << "handle" << static_cast<int>(frame.handleType())
                 << "size" << frame.width() << "x" << frame.height();

        QVideoFrame mappedFrame(frame);
        const bool mapOk = mappedFrame.map(QVideoFrame::ReadOnly);
        qDebug() << "VideoDiag   map(ReadOnly)" << mapOk
                 << "mappedBytes" << (mapOk ? mappedFrame.mappedBytes(0) : 0);
        if (mapOk)
            mappedFrame.unmap();

        const QImage img = VideoCompat::frameToImage(frame);
        qDebug() << "VideoDiag   toImage isNull" << img.isNull()
                 << "format" << static_cast<int>(img.format())
                 << "size" << img.width() << "x" << img.height();

        if (!img.isNull() && !s_diagDumped) {
            s_diagDumped = img.save(QStringLiteral("/tmp/qt6_frame_dump.png"));
            qDebug() << "VideoDiag   dump /tmp/qt6_frame_dump.png" << s_diagDumped;
        }
    }

    if (m_imagePortConnectionCount <= 0)
        return;

    const QImage image = VideoCompat::frameToImage(frame);
    if (image.isNull())
        return;

    m_output = std::make_shared<ImageData>(image);
    Q_EMIT dataUpdated(1);
#else
    const QImage image = VideoCompat::frameToImage(frame);
    if (image.isNull())
        return;

    m_output = std::make_shared<ImageData>(image);
    Q_EMIT dataUpdated(0);
#endif
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
