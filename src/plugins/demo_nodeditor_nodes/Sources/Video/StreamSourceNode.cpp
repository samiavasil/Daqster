#include "StreamSourceNode.h"

#include "NodeDataTypes/ImageData.h"

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
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &StreamSourceNode::onPlayerError);
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
        return 1;
    default:
        return 0;
    }
}

NodeDataType StreamSourceNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return ImageData().type();
}

std::shared_ptr<NodeData> StreamSourceNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void StreamSourceNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(data);
    Q_UNUSED(portIndex);
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
    if (urlString.isEmpty()) {
        setStatus(tr("Enter a stream URL first"), false);
        return;
    }

    const QUrl url(urlString);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || scheme.isEmpty()) {
        setStatus(tr("Invalid stream URL"), false);
        return;
    }
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")
        && scheme != QStringLiteral("rtsp")) {
        setStatus(tr("Unsupported stream scheme: %1").arg(url.scheme()), false);
        return;
    }

    VideoCompat::setMediaSource(m_player, url);
    m_player->play();
}

void StreamSourceNode::onFrameAvailable(const QVideoFrame &frame)
{
    const QImage image = VideoCompat::frameToImage(frame);
    if (image.isNull())
        return;

    m_output = std::make_shared<ImageData>(image);
    Q_EMIT dataUpdated(0);
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
