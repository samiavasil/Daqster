#include "FrameWindowNode.h"

#include "NodeDataTypes/VideoFrameData.h"

#include <algorithm>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {
constexpr int kMinWindow = 2;
constexpr int kMaxWindow = 32;
constexpr int kMinIntervalMs = 1;
constexpr int kMaxIntervalMs = 60000;
} // namespace

FrameWindowNode::FrameWindowNode()
{
    m_clock.start();
    // Initial config emit so the widget (when built) gets the default state.
    Q_EMIT paramsChanged(static_cast<int>(m_emitMode), m_window, m_intervalMs);
}

FrameWindowNode::~FrameWindowNode() = default;

QJsonObject FrameWindowNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    QString mode = QStringLiteral("full");
    if (m_emitMode == EmitMode::EveryFrame)
        mode = QStringLiteral("everyFrame");
    else if (m_emitMode == EmitMode::Interval)
        mode = QStringLiteral("interval");
    obj[QStringLiteral("emitMode")] = mode;
    obj[QStringLiteral("window")] = m_window;
    obj[QStringLiteral("intervalMs")] = m_intervalMs;
    return obj;
}

void FrameWindowNode::load(QJsonObject const &p)
{
    const QString mode = p.value(QStringLiteral("emitMode")).toString();
    if (mode == QStringLiteral("everyFrame"))
        m_emitMode = EmitMode::EveryFrame;
    else if (mode == QStringLiteral("interval"))
        m_emitMode = EmitMode::Interval;
    else
        m_emitMode = EmitMode::Full;

    m_window = std::max(kMinWindow,
                        std::min(kMaxWindow, p.value(QStringLiteral("window")).toInt(m_window)));
    m_intervalMs = std::max(kMinIntervalMs,
                            std::min(kMaxIntervalMs,
                                     p.value(QStringLiteral("intervalMs")).toInt(m_intervalMs)));

    trimWindow();
    Q_EMIT paramsChanged(static_cast<int>(m_emitMode), m_window, m_intervalMs);
}

unsigned int FrameWindowNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType FrameWindowNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portIndex);
    if (portType == PortType::Out)
        return FrameWindowData().type();
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> FrameWindowNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void FrameWindowNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    const auto frame = std::dynamic_pointer_cast<VideoFrameData>(data);
    if (!frame || !frame->hasFrame()) {
        // Null input clears the window: the old block is no longer valid.
        m_items.clear();
        m_output.reset();
        m_lastEmitMs = -1;
        Q_EMIT dataInvalidated(0);
        return;
    }

    m_items.append(FrameWindowData::Item{frame, m_clock.elapsed()});
    trimWindow();

    if (!shouldEmit())
        return; // window updated silently — no emit for this frame

    emitWindow();
}

void FrameWindowNode::onModeChanged(int mode)
{
    switch (mode) {
    case static_cast<int>(EmitMode::EveryFrame):
        m_emitMode = EmitMode::EveryFrame;
        break;
    case static_cast<int>(EmitMode::Interval):
        m_emitMode = EmitMode::Interval;
        break;
    default:
        m_emitMode = EmitMode::Full;
        break;
    }
    m_lastEmitMs = -1; // a mode change re-arms the interval gate
    Q_EMIT paramsChanged(static_cast<int>(m_emitMode), m_window, m_intervalMs);
}

void FrameWindowNode::onWindowChanged(int windowSize)
{
    m_window = std::max(kMinWindow, std::min(kMaxWindow, windowSize));
    trimWindow();
    Q_EMIT paramsChanged(static_cast<int>(m_emitMode), m_window, m_intervalMs);
}

void FrameWindowNode::onIntervalMsChanged(int intervalMs)
{
    m_intervalMs = std::max(kMinIntervalMs, std::min(kMaxIntervalMs, intervalMs));
    m_lastEmitMs = -1;
    Q_EMIT paramsChanged(static_cast<int>(m_emitMode), m_window, m_intervalMs);
}

void FrameWindowNode::trimWindow()
{
    while (m_items.size() > m_window)
        m_items.removeFirst();
}

bool FrameWindowNode::shouldEmit() const
{
    switch (m_emitMode) {
    case EmitMode::EveryFrame:
        return !m_items.isEmpty();
    case EmitMode::Full:
        return m_items.size() >= m_window;
    case EmitMode::Interval:
        if (m_items.isEmpty())
            return false;
        return m_lastEmitMs < 0 || (m_clock.elapsed() - m_lastEmitMs) >= m_intervalMs;
    }
    return false;
}

void FrameWindowNode::emitWindow()
{
    // Zero-copy: the window items hold the same shared_ptr objects the source
    // produced — the block shares them, nothing is converted or copied.
    m_output = std::make_shared<FrameWindowData>(m_items);
    m_lastEmitMs = m_clock.elapsed();
    Q_EMIT dataUpdated(0);
}
