#include "FrameSamplerNode.h"

#include "NodeDataTypes/VideoFrameData.h"

#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

FrameSamplerNode::FrameSamplerNode()
{
    buildWidget();
}

FrameSamplerNode::~FrameSamplerNode()
{
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject FrameSamplerNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    obj[QStringLiteral("mode")] = (m_mode == Mode::EveryNth)
        ? QStringLiteral("everyNth")
        : QStringLiteral("maxFps");
    obj[QStringLiteral("everyN")] = m_everyN;
    obj[QStringLiteral("maxFps")] = m_maxFps;
    return obj;
}

void FrameSamplerNode::load(QJsonObject const &p)
{
    const QString mode = p.value(QStringLiteral("mode")).toString();
    m_mode = (mode == QStringLiteral("maxFps")) ? Mode::MaxFps : Mode::EveryNth;
    m_everyN = std::max(1, std::min(1000, p.value(QStringLiteral("everyN")).toInt(m_everyN)));
    m_maxFps = std::max(1, std::min(120, p.value(QStringLiteral("maxFps")).toInt(m_maxFps)));

    resetGate();
    syncWidgetsFromParams();
}

unsigned int FrameSamplerNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType FrameSamplerNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> FrameSamplerNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void FrameSamplerNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    m_lastInput = std::dynamic_pointer_cast<VideoFrameData>(data);

    if (!m_lastInput || !m_lastInput->hasFrame()) {
        m_output.reset();
        Q_EMIT dataInvalidated(0);
        return;
    }

    if (!passesGate())
        return; // dropped frame — no emit

    // Zero-copy passthrough: share the same VideoFrameData (ref-count bump).
    m_output = m_lastInput;
    Q_EMIT dataUpdated(0);
}

QWidget *FrameSamplerNode::embeddedWidget()
{
    return m_widget;
}

void FrameSamplerNode::buildWidget()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    m_modeCombo = new QComboBox(m_widget);
    m_modeCombo->addItem(tr("Every N-th frame"), static_cast<int>(Mode::EveryNth));
    m_modeCombo->addItem(tr("Max FPS"), static_cast<int>(Mode::MaxFps));
    m_modeCombo->setCurrentIndex(static_cast<int>(m_mode));
    layout->addWidget(m_modeCombo);

    m_everyNSpin = new QSpinBox(m_widget);
    m_everyNSpin->setRange(1, 1000);
    m_everyNSpin->setValue(m_everyN);
    m_everyNSpin->setPrefix(tr("N = "));
    layout->addWidget(m_everyNSpin);

    m_maxFpsSpin = new QSpinBox(m_widget);
    m_maxFpsSpin->setRange(1, 120);
    m_maxFpsSpin->setValue(m_maxFps);
    m_maxFpsSpin->setPrefix(tr("Max FPS = "));
    layout->addWidget(m_maxFpsSpin);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                m_mode = static_cast<Mode>(index);
                resetGate();
                updateSpinVisibility();
            });
    connect(m_everyNSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value) {
                m_everyN = value;
                resetGate();
            });
    connect(m_maxFpsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value) {
                m_maxFps = value;
                resetGate();
            });

    updateSpinVisibility();
}

void FrameSamplerNode::resetGate()
{
    m_frameCounter = 0;
    m_fpsTimer.invalidate();
}

bool FrameSamplerNode::passesGate()
{
    if (m_mode == Mode::EveryNth) {
        ++m_frameCounter;
        return (m_everyN <= 1) || (m_frameCounter % m_everyN == 0);
    }

    // MaxFps: the timer starts on the first frame; a frame passes once the
    // interval (1e9 / maxFps ns) has elapsed, then the timer restarts.
    if (!m_fpsTimer.isValid())
        m_fpsTimer.start();
    const qint64 intervalNs = (m_maxFps > 0) ? (1000000000LL / m_maxFps) : 0;
    if (m_fpsTimer.nsecsElapsed() >= intervalNs) {
        m_fpsTimer.restart();
        return true;
    }
    return false;
}

void FrameSamplerNode::syncWidgetsFromParams()
{
    if (m_modeCombo) {
        const QSignalBlocker blocker(m_modeCombo);
        m_modeCombo->setCurrentIndex(static_cast<int>(m_mode));
    }
    if (m_everyNSpin) {
        const QSignalBlocker blocker(m_everyNSpin);
        m_everyNSpin->setValue(m_everyN);
    }
    if (m_maxFpsSpin) {
        const QSignalBlocker blocker(m_maxFpsSpin);
        m_maxFpsSpin->setValue(m_maxFps);
    }
    updateSpinVisibility();
}

void FrameSamplerNode::updateSpinVisibility()
{
    if (m_everyNSpin)
        m_everyNSpin->setVisible(m_mode == Mode::EveryNth);
    if (m_maxFpsSpin)
        m_maxFpsSpin->setVisible(m_mode == Mode::MaxFps);
}