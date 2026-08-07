#include "VideoOutputNode.h"

#include "NodeDataTypes/ImageData.h"

#include <QEvent>
#include <QLabel>
#include <QPixmap>
#include <QSize>
#include <QVBoxLayout>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoOutputNode::VideoOutputNode()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);

    m_label = new QLabel(m_widget);
    m_label->setMinimumSize(320, 240);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setText(tr("No video input"));
    m_label->setStyleSheet(QStringLiteral("background-color: black; color: gray;"));
    layout->addWidget(m_label);

    m_label->installEventFilter(this);
}

VideoOutputNode::~VideoOutputNode()
{
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject VideoOutputNode::save() const
{
    return QtNodes::NodeDelegateModel::save();
}

void VideoOutputNode::load(QJsonObject const &p)
{
    Q_UNUSED(p);
}

unsigned int VideoOutputNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoOutputNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return ImageData().type();
}

std::shared_ptr<NodeData> VideoOutputNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void VideoOutputNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    m_image = QImage();
    m_output.reset();

    auto imageData = std::dynamic_pointer_cast<ImageData>(data);
    if (imageData && !imageData->isEmpty()) {
        m_image = imageData->image();
        m_output = imageData;
        updateDisplay();
        Q_EMIT dataUpdated(0);
    } else {
        updateDisplay();
        Q_EMIT dataInvalidated(0);
    }
}

QWidget *VideoOutputNode::embeddedWidget()
{
    return m_widget;
}

bool VideoOutputNode::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_label && event->type() == QEvent::Resize)
        updateDisplay();
    return false;
}

void VideoOutputNode::updateDisplay()
{
    if (m_image.isNull()) {
        m_label->setPixmap(QPixmap());
        return;
    }

    QSize targetSize = m_label->size();
    if (!targetSize.isValid() || targetSize.isEmpty())
        targetSize = m_label->minimumSize();

    const QPixmap pixmap = QPixmap::fromImage(m_image);
    m_label->setPixmap(pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
