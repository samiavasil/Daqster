#include "FrameWindowDisplayNode.h"

#include "Displays/DisplayRefresh/DisplayRefreshDriver.h"
#include "NodeDataTypes/FrameWindowData.h"

#include <QMetaMethod>
#include <QMetaObject>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;
using QtNodes::PortIndex;
using QtNodes::PortType;

void FrameWindowDisplayBridge::onComputeDone(FrameWindowDisplaySnapshot snapshot)
{
    // Queued to the GUI thread. Forward only — the widget owns the labels.
    if (m_node != nullptr)
        Q_EMIT m_node->snapshotReady(snapshot);
}

FrameWindowDisplayNode::FrameWindowDisplayNode()
{
    // Display nodes must never get a graphics effect (perf).
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    qRegisterMetaType<FrameWindowDisplaySnapshot>("FrameWindowDisplaySnapshot");

    m_clock.start();
    m_bridge = new FrameWindowDisplayBridge(this);
    m_driver = new DisplayRefreshDriver(this);
    connect(m_driver, &DisplayRefreshDriver::refreshRequested,
            this, &FrameWindowDisplayNode::onRefreshRequested);
}

FrameWindowDisplayNode::~FrameWindowDisplayNode()
{
    stop();

    if (m_bridge != nullptr) {
        delete m_bridge;
        m_bridge = nullptr;
    }
}

void FrameWindowDisplayNode::stop()
{
    if (m_driver != nullptr)
        m_driver->stop();
}

unsigned int FrameWindowDisplayNode::nPorts(PortType portType) const
{
    return portType == PortType::In ? 1u : 0u;
}

NodeDataType FrameWindowDisplayNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return FrameWindowData().type();
}

std::shared_ptr<NodeData> FrameWindowDisplayNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return nullptr;
}

void FrameWindowDisplayNode::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (portIndex != 0)
        return;

    // The window is a list of shared frame pointers — copying it bumps the
    // reference counts only; no pixel buffer is touched.
    const std::shared_ptr<const FrameWindowData> window =
        std::dynamic_pointer_cast<const FrameWindowData>(data);

    NodeValidationState s;
    if (window && !window->isEmpty()) {
        m_messages += 1;
        m_lastWindow = window;
        m_lastArrivalMs = m_clock.elapsed();
        s._state = NodeValidationState::State::Valid;
        m_driver->markDirty();
    } else if (window) {
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("Frame window is empty");
    } else if (data) {
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("Expected FrameWindowData (\"frame-window\")");
    } else {
        m_lastWindow.reset();
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("No data connected");
    }
    setValidationState(s);
}

FrameWindowDisplaySnapshot FrameWindowDisplayNode::buildSnapshot(const FrameWindowData &window,
                                                                 qint64 messages,
                                                                 qint64 msSinceLast,
                                                                 int maxThumbnails,
                                                                 int thumbnailWidth)
{
    FrameWindowDisplaySnapshot snapshot;
    snapshot.count = window.count();
    snapshot.firstTimestampMs = window.firstTimestampMs();
    snapshot.lastTimestampMs = window.lastTimestampMs();
    snapshot.spanMs = (snapshot.firstTimestampMs >= 0 && snapshot.lastTimestampMs >= 0)
                          ? snapshot.lastTimestampMs - snapshot.firstTimestampMs
                          : 0;
    snapshot.messages = messages;
    snapshot.msSinceLast = msSinceLast;

    const QVector<FrameWindowData::Item> &items = window.items();
    const int wanted = qMin(qMax(0, maxThumbnails), items.size());

    // Newest first when picking (the last frames are the ones of interest),
    // but the thumbnails are appended oldest→newest so the row reads in time
    // order like the window itself.
    const int first = items.size() - wanted;
    for (int i = first; i < items.size(); ++i) {
        const std::shared_ptr<VideoFrameData> &frame = items.at(i).frame;
        if (!frame || !frame->hasFrame()) {
            ++snapshot.skipped;
            continue;
        }

        // GPU-resident payload: asImage() is a GL readback and therefore
        // GUI-thread only. Converting here would break the threading contract,
        // so the frame is reported as skipped (metadata only) instead.
        if (frame->isGpuResident()) {
            ++snapshot.skipped;
            continue;
        }

        const QImage thumb =
            VideoFrameData::frameToImageScaled(frame->frame(), thumbnailWidth);
        if (thumb.isNull()) {
            // Unsupported pixel format / failed map — again metadata only.
            ++snapshot.skipped;
            continue;
        }
        snapshot.thumbnails.append(thumb);
    }

    snapshot.metadataOnly = snapshot.thumbnails.isEmpty();
    return snapshot;
}

void FrameWindowDisplayNode::onRefreshRequested()
{
    // No consumer yet (headless / no widget): stay dirty, see TextDisplayNode.
    if (!isSignalConnected(QMetaMethod::fromSignal(&FrameWindowDisplayNode::snapshotReady))) {
        m_driver->markDirty();
        return;
    }
    if (!m_lastWindow) {
        return;
    }

    // Hand the window (pointers only) to the worker and drop our own hold: an
    // idle display keeps neither the window nor its frames alive.
    const std::shared_ptr<const FrameWindowData> window = m_lastWindow;
    m_lastWindow.reset();
    const qint64 messages = m_messages;
    const qint64 sinceLast = m_lastArrivalMs < 0 ? -1
                                                 : m_clock.elapsed() - m_lastArrivalMs;

    m_driver->submit([token = m_driver->token(), bridge = m_bridge, window, messages,
                      sinceLast]() {
        if (Daqster::isCancelled(token))
            return;
        if (!window)
            return;

        const FrameWindowDisplaySnapshot snapshot =
            buildSnapshot(*window, messages, sinceLast);

        // Do not post after shutdown has begun — the bridge may be gone.
        if (Daqster::isCancelled(token))
            return;
        QMetaObject::invokeMethod(bridge, "onComputeDone", Qt::QueuedConnection,
                                  Q_ARG(FrameWindowDisplaySnapshot, snapshot));
    });
}
