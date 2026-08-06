#ifndef VIDEOMODIFIERNODE_H
#define VIDEOMODIFIERNODE_H

#include <QtNodes/NodeDelegateModel>

#include <memory>

class QLabel;
class QWidget;

class ImageData;

/**
 * @brief Video modifier node: transforms incoming ImageData frames.
 *
 * Demo effect: swaps the red and blue channels of every frame (R<->B) using a
 * QImage pixel operation, then emits the modified frame as ImageData ("image").
 */
class VideoModifierNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    VideoModifierNode();
    ~VideoModifierNode() override;

    QString caption() const override
    { return QStringLiteral("Video Modifier"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("VideoModifier"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

private:
    QImage swapRedBlueChannels(const QImage &image) const;

    QWidget *m_widget = nullptr;
    QLabel *m_infoLabel = nullptr;
    std::shared_ptr<ImageData> m_output;
};

#endif // VIDEOMODIFIERNODE_H
