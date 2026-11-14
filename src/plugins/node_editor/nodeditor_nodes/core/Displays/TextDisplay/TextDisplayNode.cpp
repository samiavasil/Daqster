#include "TextDisplayNode.h"

#include "Displays/DisplayRefresh/DisplayRefreshDriver.h"
#include "NodeDataTypes/TextData.h"

#include <QMetaMethod>
#include <QMetaObject>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;
using QtNodes::PortIndex;
using QtNodes::PortType;

void TextDisplayBridge::onComputeDone(TextDisplaySnapshot snapshot)
{
    // Queued to the GUI thread. Forward only — the widget owns the QLabel.
    if (m_node != nullptr)
        Q_EMIT m_node->snapshotReady(snapshot);
}

TextDisplayNode::TextDisplayNode()
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback.
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    qRegisterMetaType<TextDisplaySnapshot>("TextDisplaySnapshot");

    m_clock.start();
    m_bridge = new TextDisplayBridge(this);
    m_driver = new DisplayRefreshDriver(this);
    connect(m_driver, &DisplayRefreshDriver::refreshRequested,
            this, &TextDisplayNode::onRefreshRequested);
}

TextDisplayNode::~TextDisplayNode()
{
    stop();

    if (m_bridge != nullptr) {
        delete m_bridge;
        m_bridge = nullptr;
    }
}

void TextDisplayNode::stop()
{
    if (m_driver != nullptr)
        m_driver->stop();
}

unsigned int TextDisplayNode::nPorts(PortType portType) const
{
    // Consumer: one input, zero outputs.
    return portType == PortType::In ? 1u : 0u;
}

NodeDataType TextDisplayNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return TextData().type();
}

std::shared_ptr<NodeData> TextDisplayNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return nullptr;
}

void TextDisplayNode::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (portIndex != 0)
        return;

    // Const access only: read the text, drop the payload.
    const std::shared_ptr<TextData> text = std::dynamic_pointer_cast<TextData>(data);

    NodeValidationState s;
    if (text) {
        m_messages += 1;
        m_characters += text->text().size();
        // Kept for the tooltip (implicit sharing — no characters copied) and
        // to have the payload after the producer has moved on. Nothing else of
        // the TextData is retained.
        m_lastText = text->text();
        m_lastArrivalMs = m_clock.elapsed();
        s._state = NodeValidationState::State::Valid;
        m_driver->markDirty();
    } else if (data) {
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("Expected TextData (\"text\")");
    } else {
        // Counters stay: they describe the stream so far, not the connection.
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("No data connected");
    }
    setValidationState(s);
}

TextDisplaySnapshot TextDisplayNode::buildSnapshot(const QString &text, qint64 messages,
                                                   qint64 characters, qint64 msSinceLast,
                                                   int previewChars)
{
    TextDisplaySnapshot snapshot;
    snapshot.messages = messages;
    snapshot.characters = characters;
    snapshot.msSinceLast = msSinceLast;
    snapshot.full = text;
    snapshot.lines = text.isEmpty() ? 0 : text.count(QLatin1Char('\n')) + 1;

    const int cap = qMax(1, previewChars);
    if (text.size() > cap) {
        // left() cuts on a UTF-16 boundary: a surrogate pair at the end may be
        // halved, which renders as a replacement glyph — acceptable for a
        // preview, and the full text stays in the tooltip.
        snapshot.preview = text.left(cap) + QStringLiteral("…");
        snapshot.truncated = true;
    } else {
        snapshot.preview = text;
    }
    return snapshot;
}

void TextDisplayNode::onRefreshRequested()
{
    // Nobody consumes the snapshot (headless, or no widget attached yet): stay
    // dirty so the first widget that connects still gets a render. Same policy
    // as DaqDisplayNode (REQ-SW-PL-056).
    if (!isSignalConnected(QMetaMethod::fromSignal(&TextDisplayNode::snapshotReady))) {
        m_driver->markDirty();
        return;
    }

    // Snapshot the cheap state on the GUI thread; the worker only formats it.
    const QString text = m_lastText;               // implicit share
    const qint64 messages = m_messages;
    const qint64 characters = m_characters;
    const qint64 sinceLast = m_lastArrivalMs < 0 ? -1
                                                 : m_clock.elapsed() - m_lastArrivalMs;

    // token + bridge BY VALUE: the node may be destroyed while the task runs.
    m_driver->submit([token = m_driver->token(), bridge = m_bridge, text, messages,
                      characters, sinceLast]() {
        if (Daqster::isCancelled(token))
            return;

        const TextDisplaySnapshot snapshot =
            buildSnapshot(text, messages, characters, sinceLast);

        // Do not post after shutdown has begun — the bridge may be gone.
        if (Daqster::isCancelled(token))
            return;
        QMetaObject::invokeMethod(bridge, "onComputeDone",
                                  Qt::QueuedConnection, Q_ARG(TextDisplaySnapshot, snapshot));
    });
}
