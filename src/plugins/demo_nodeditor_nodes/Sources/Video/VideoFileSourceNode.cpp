#include "VideoFileSourceNode.h"

#include "NodeDataTypes/ImageData.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QPushButton>
#include <QVBoxLayout>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoFileSourceNode::VideoFileSourceNode()
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
            this, &VideoFileSourceNode::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &VideoFileSourceNode::onPlayerError);
}

VideoFileSourceNode::~VideoFileSourceNode()
{
    if (m_player != nullptr)
        m_player->stop();
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject VideoFileSourceNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["filePath"] = m_fileEdit->text();
    return modelJson;
}

void VideoFileSourceNode::load(QJsonObject const &p)
{
    if (p.contains("filePath"))
        m_fileEdit->setText(p["filePath"].toString());
}

unsigned int VideoFileSourceNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoFileSourceNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return ImageData().type();
}

std::shared_ptr<NodeData> VideoFileSourceNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void VideoFileSourceNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(data);
    Q_UNUSED(portIndex);
}

QWidget *VideoFileSourceNode::embeddedWidget()
{
    return m_widget;
}

void VideoFileSourceNode::buildWidget()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *fileRow = new QHBoxLayout();
    m_fileEdit = new QLineEdit(m_widget);
    m_fileEdit->setPlaceholderText(tr("Path to video file"));
    auto *browseButton = new QPushButton(tr("..."), m_widget);
    browseButton->setMaximumWidth(32);
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(browseButton);
    layout->addLayout(fileRow);

    auto *controlRow = new QHBoxLayout();
    m_playPauseButton = new QPushButton(tr("Play"), m_widget);
    m_statusLabel = new QLabel(tr("Stopped"), m_widget);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    controlRow->addWidget(m_playPauseButton);
    controlRow->addWidget(m_statusLabel, 1);
    layout->addLayout(controlRow);

    connect(browseButton, &QPushButton::clicked,
            this, &VideoFileSourceNode::onBrowseClicked);
    connect(m_playPauseButton, &QPushButton::clicked,
            this, &VideoFileSourceNode::onPlayPauseClicked);
}

void VideoFileSourceNode::onBrowseClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        m_widget,
        tr("Select video file"),
        QString(),
        tr("Video files (*.mp4 *.avi *.mkv *.mov *.webm *.m4v *.mpg *.mpeg);;All files (*)"));

    if (!filePath.isEmpty())
        m_fileEdit->setText(QDir::toNativeSeparators(filePath));
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
}

void VideoFileSourceNode::onFrameAvailable(const QVideoFrame &frame)
{
    const QImage image = VideoCompat::frameToImage(frame);
    if (image.isNull())
        return;

    m_output = std::make_shared<ImageData>(image);
    Q_EMIT dataUpdated(0);
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
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok ? QStringLiteral("color: green;")
                                    : QStringLiteral("color: gray;"));
}

QString VideoFileSourceNode::currentFilePath() const
{
    return m_fileEdit->text().trimmed();
}

void VideoFileSourceNode::updatePlayButton()
{
    m_playPauseButton->setText(m_isPlaying ? tr("Pause") : tr("Play"));
}
