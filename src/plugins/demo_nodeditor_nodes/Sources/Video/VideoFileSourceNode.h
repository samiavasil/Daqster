#ifndef VIDEOFILESOURCENODE_H
#define VIDEOFILESOURCENODE_H

#include "VideoCompat.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

class QLabel;
class QLineEdit;
class QMediaPlayer;
class QPushButton;
class QWidget;

class ImageData;

/**
 * @brief Video file source node: plays a local video file and emits frames.
 *
 * Uses QMediaPlayer with a VideoCompat frame probe attached. Each decoded
 * frame is converted to QImage and emitted as ImageData ("image") downstream.
 */
class VideoFileSourceNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    VideoFileSourceNode();
    ~VideoFileSourceNode() override;

    QString caption() const override
    { return QStringLiteral("Video File Source"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("VideoFileSource"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

private slots:
    void onBrowseClicked();
    void onPlayPauseClicked();
    void onFrameAvailable(const QVideoFrame &frame);
    void onPlaybackStateChanged(int state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error, const QString &errorString);

private:
    void buildWidget();
    void setStatus(const QString &text, bool ok);
    QString currentFilePath() const;
    void updatePlayButton();

    QWidget *m_widget = nullptr;
    QLineEdit *m_fileEdit = nullptr;
    QPushButton *m_playPauseButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QMediaPlayer *m_player = nullptr;
    VideoCompat::FrameProbe *m_frameProbe = nullptr;
    std::shared_ptr<ImageData> m_output;
    QString m_loadedPath;
    bool m_isPlaying = false;
};

#endif // VIDEOFILESOURCENODE_H
