#include "VideoModifierNode.h"

#include "NodeDataTypes/ImageData.h"

#include <QImage>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoModifierNode::VideoModifierNode()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);

    m_infoLabel = new QLabel(tr("Swaps red and blue channels (R<->B) on every frame"), m_widget);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setMinimumWidth(180);
    layout->addWidget(m_infoLabel);
}

VideoModifierNode::~VideoModifierNode()
{
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject VideoModifierNode::save() const
{
    return QtNodes::NodeDelegateModel::save();
}

void VideoModifierNode::load(QJsonObject const &p)
{
    Q_UNUSED(p);
}

unsigned int VideoModifierNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoModifierNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return ImageData().type();
}

std::shared_ptr<NodeData> VideoModifierNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void VideoModifierNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    m_output.reset();

    auto imageData = std::dynamic_pointer_cast<ImageData>(data);
    if (!imageData || imageData->isEmpty()) {
        Q_EMIT dataInvalidated(0);
        return;
    }

    const QImage swapped = swapRedBlueChannels(imageData->image());
    if (swapped.isNull()) {
        Q_EMIT dataInvalidated(0);
        return;
    }

    m_output = std::make_shared<ImageData>(swapped);
    Q_EMIT dataUpdated(0);
}

QWidget *VideoModifierNode::embeddedWidget()
{
    return m_widget;
}

QImage VideoModifierNode::swapRedBlueChannels(const QImage &image) const
{
    if (image.isNull())
        return QImage();

    QImage result;
    if (image.format() == QImage::Format_RGB32) {
        result = image;    // implicit share — detach before mutating
        result.detach();
    } else {
        result = image.convertToFormat(QImage::Format_RGB32);
    }

    // QImage::Format_RGB32 stores each 32-bit pixel as [x][R][G][B] on
    // big-endian and [B][G][R][x] on little-endian. Swap the red and blue
    // bytes in every pixel to produce the R<->B channel swap.
    for (int y = 0; y < result.height(); ++y) {
        uchar *line = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            uchar *pixel = line + (x * 4);
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
            std::swap(pixel[1], pixel[3]);
#else
            std::swap(pixel[0], pixel[2]);
#endif
        }
    }

    return result;
}
