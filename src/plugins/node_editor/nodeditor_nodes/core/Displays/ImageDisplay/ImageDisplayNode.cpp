#include "ImageDisplayNode.h"

#include "Displays/DisplayRefresh/DisplayRefreshDriver.h"
#include "NodeDataTypes/ImageData.h"

#include <QMetaMethod>
#include <QMetaObject>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;
using QtNodes::PortIndex;
using QtNodes::PortType;

void ImageDisplayBridge::onComputeDone(ImageDisplaySnapshot snapshot)
{
    // Queued to the GUI thread. Forward only — the widget owns the QLabel.
    if (m_node != nullptr)
        Q_EMIT m_node->snapshotReady(snapshot);
}

ImageDisplayNode::ImageDisplayNode()
{
    // Display nodes must never get a graphics effect (perf).
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    qRegisterMetaType<ImageDisplaySnapshot>("ImageDisplaySnapshot");

    m_clock.start();
    m_bridge = new ImageDisplayBridge(this);
    m_driver = new DisplayRefreshDriver(this);
    connect(m_driver, &DisplayRefreshDriver::refreshRequested,
            this, &ImageDisplayNode::onRefreshRequested);
}

ImageDisplayNode::~ImageDisplayNode()
{
    stop();

    if (m_bridge != nullptr) {
        delete m_bridge;
        m_bridge = nullptr;
    }
}

void ImageDisplayNode::stop()
{
    if (m_driver != nullptr)
        m_driver->stop();
}

unsigned int ImageDisplayNode::nPorts(PortType portType) const
{
    return portType == PortType::In ? 1u : 0u;
}

NodeDataType ImageDisplayNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return ImageData().type();
}

std::shared_ptr<NodeData> ImageDisplayNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return nullptr;
}

void ImageDisplayNode::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (portIndex != 0)
        return;

    // Const access: the payload is stored under a const pointer, so no pass
    // can write into the producer's pixels.
    const std::shared_ptr<const ImageData> image =
        std::dynamic_pointer_cast<const ImageData>(data);

    NodeValidationState s;
    if (image && !image->isEmpty()) {
        m_messages += 1;
        m_lastData = image;
        m_lastSize = QSize(image->width(), image->height());
        m_lastArrivalMs = m_clock.elapsed();
        s._state = NodeValidationState::State::Valid;
        m_driver->markDirty();
    } else if (image) {
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("ImageData is empty");
    } else if (data) {
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("Expected ImageData (\"image\")");
    } else {
        m_lastData.reset();
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("No data connected");
    }
    setValidationState(s);
}

QString ImageDisplayNode::formatName(QImage::Format format)
{
    // Explicit names instead of cast-to-int: the enum values are NOT stable
    // across Qt versions (Qt 6 added/reordered formats), so a number would lie.
    switch (format) {
    case QImage::Format_Invalid: return QStringLiteral("invalid");
    case QImage::Format_Mono: return QStringLiteral("mono");
    case QImage::Format_MonoLSB: return QStringLiteral("mono-lsb");
    case QImage::Format_Indexed8: return QStringLiteral("indexed8");
    case QImage::Format_RGB32: return QStringLiteral("RGB32");
    case QImage::Format_ARGB32: return QStringLiteral("ARGB32");
    case QImage::Format_ARGB32_Premultiplied: return QStringLiteral("ARGB32-pm");
    case QImage::Format_RGB16: return QStringLiteral("RGB16");
    case QImage::Format_RGB888: return QStringLiteral("RGB888");
    case QImage::Format_RGBX8888: return QStringLiteral("RGBX8888");
    case QImage::Format_RGBA8888: return QStringLiteral("RGBA8888");
    case QImage::Format_RGBA8888_Premultiplied: return QStringLiteral("RGBA8888-pm");
    case QImage::Format_Grayscale8: return QStringLiteral("grayscale8");
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    case QImage::Format_BGR888: return QStringLiteral("BGR888");
#else
    case QImage::Format_RGB666: return QStringLiteral("RGB666");
    case QImage::Format_ARGB6666_Premultiplied: return QStringLiteral("ARGB6666-pm");
#endif
    default: break;
    }
    return QStringLiteral("format %1").arg(static_cast<int>(format));
}

ImageDisplaySnapshot ImageDisplayNode::buildSnapshot(const QImage &image,
                                                     const QString &mimeType,
                                                     const QString &model,
                                                     qint64 metaElapsedMs,
                                                     qint64 messages,
                                                     qint64 msSinceLast,
                                                     int maxWidth,
                                                     int maxHeight)
{
    ImageDisplaySnapshot snapshot;
    snapshot.width = image.width();
    snapshot.height = image.height();
    snapshot.format = formatName(image.format());
    snapshot.mimeType = mimeType;
    snapshot.model = model;
    snapshot.metaElapsedMs = metaElapsedMs;
    snapshot.messages = messages;
    snapshot.msSinceLast = msSinceLast;

    if (image.isNull() || maxWidth <= 0 || maxHeight <= 0)
        return snapshot;

    if (image.width() > maxWidth || image.height() > maxHeight) {
        // scaled() allocates a new buffer: the result does not alias the
        // producer's pixels, so the display never pins a full-resolution frame.
        snapshot.thumbnail = image.scaled(QSize(maxWidth, maxHeight),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
        snapshot.downscaled = true;
    } else {
        // Small image: copy (not the shared buffer) for the same reason —
        // dropping the snapshot drops the last reference to the pixels.
        snapshot.thumbnail = image.copy();
    }
    return snapshot;
}

void ImageDisplayNode::onRefreshRequested()
{
    // No consumer yet (headless / no widget): stay dirty, see TextDisplayNode.
    if (!isSignalConnected(QMetaMethod::fromSignal(&ImageDisplayNode::snapshotReady))) {
        m_driver->markDirty();
        return;
    }
    if (!m_lastData) {
        return;
    }

    // The payload is handed to the worker as an immutable shared_ptr; the node
    // drops its own reference immediately, so an idle display holds only the
    // thumbnail the widget shows.
    const std::shared_ptr<const ImageData> image = m_lastData;
    m_lastData.reset();
    const qint64 messages = m_messages;
    const qint64 sinceLast = m_lastArrivalMs < 0 ? -1
                                                 : m_clock.elapsed() - m_lastArrivalMs;

    m_driver->submit([token = m_driver->token(), bridge = m_bridge, image, messages,
                      sinceLast]() {
        if (Daqster::isCancelled(token))
            return;
        if (!image)
            return;

        const ImageData::Meta &meta = image->meta();
        const ImageDisplaySnapshot snapshot = buildSnapshot(image->image(),
                                                            meta.mimeType,
                                                            meta.model,
                                                            meta.elapsedMs,
                                                            messages,
                                                            sinceLast);

        // Do not post after shutdown has begun — the bridge may be gone.
        if (Daqster::isCancelled(token))
            return;
        QMetaObject::invokeMethod(bridge, "onComputeDone", Qt::QueuedConnection,
                                  Q_ARG(ImageDisplaySnapshot, snapshot));
    });
}
