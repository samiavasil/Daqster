#ifndef STREAMSOURCENODE_H
#define STREAMSOURCENODE_H

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
 * @brief Stream source node: plays an HTTP/RTSP video stream and emits frames.
 *
 * Uses QMediaPlayer with a VideoCompat frame probe attached. Each decoded
 * frame is converted to QImage and emitted as ImageData ("image") downstream.
 */
class StreamSourceNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    StreamSourceNode();
    ~StreamSourceNode() override;

    QString caption() const override
    { return QStringLiteral("Stream Source"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("StreamSource"); }

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
    void onConnectClicked();
    void onFrameAvailable(const QVideoFrame &frame);
    void onPlaybackStateChanged(int state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error, const QString &errorString);

private:
    void buildWidget();
    void setStatus(const QString &text, bool ok);
    void updateConnectButton();

    QWidget *m_widget = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QMediaPlayer *m_player = nullptr;
    VideoCompat::FrameProbe *m_frameProbe = nullptr;
    std::shared_ptr<ImageData> m_output;
    bool m_isPlaying = false;
};

#endif // STREAMSOURCENODE_H
