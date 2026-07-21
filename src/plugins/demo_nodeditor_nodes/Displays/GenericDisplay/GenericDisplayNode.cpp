#include "GenericDisplayNode.h"
#include "QtChartsCompat.h"
#include <GenericNumericTypes.h>
#include <QDebug>

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

GenericDisplayNode::GenericDisplayNode()
{
    setupUi();
}

GenericDisplayNode::~GenericDisplayNode()
{
}

void GenericDisplayNode::setupUi()
{
    m_stack = new QStackedWidget();

    // Page 0: Config panel
    auto* configPanel = new QWidget();
    auto* configLayout = new QFormLayout(configPanel);

    auto* channelSpin = new QSpinBox();
    channelSpin->setRange(1, 16);
    channelSpin->setValue(1);
    configLayout->addRow("Channels:", channelSpin);

    auto* sampleRateSpin = new QSpinBox();
    sampleRateSpin->setRange(1, 1000000);
    sampleRateSpin->setValue(44100);
    configLayout->addRow("Sample Rate:", sampleRateSpin);

    auto* typeCombo = new QComboBox();
    typeCombo->addItem("INT16", 0);
    typeCombo->addItem("UINT16", 1);
    typeCombo->addItem("INT32", 2);
    typeCombo->addItem("UINT32", 3);
    typeCombo->addItem("FLOAT32", 4);
    typeCombo->addItem("FLOAT64", 5);
    configLayout->addRow("Sample Type:", typeCombo);

    auto* applyBtn = new QPushButton("Apply");
    configLayout->addRow(applyBtn);

    m_configPanelIndex = m_stack->addWidget(configPanel);

    // Page 1: Time chart
    m_timeChart = new QtChartsCompat::ChartView(new QtChartsCompat::Chart());
    m_timeChart->chart()->setTitle("Time Domain");
    m_timeChart->setMinimumSize(400, 200);
    m_timeChartIndex = m_stack->addWidget(m_timeChart);

    // Page 2: FFT chart
    m_fftChart = new QtChartsCompat::ChartView(new QtChartsCompat::Chart());
    m_fftChart->chart()->setTitle("Frequency Spectrum");
    m_fftChart->setMinimumSize(400, 200);
    m_fftChartIndex = m_stack->addWidget(m_fftChart);

    m_stack->setCurrentIndex(m_configPanelIndex);
}

QJsonObject GenericDisplayNode::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    return modelJson;
}

unsigned int GenericDisplayNode::nPorts(PortType portType) const
{
    unsigned int num = 0;
    switch (portType) {
    case QtNodes::PortType::In:
        num = 1;
        break;
    default:
        break;
    }
    return num;
}

NodeDataType GenericDisplayNode::dataType(PortType portType, PortIndex portIndex) const
{
    return {"generic_numeric", "Generic"};
}

std::shared_ptr<NodeData> GenericDisplayNode::outData(PortIndex port)
{
    return nullptr;
}

void GenericDisplayNode::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (portIndex != 0 || !data) return;

    auto numericData = std::dynamic_pointer_cast<GenericNumericData>(data);
    if (!numericData) {
        NodeValidationState s;
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = "Expected GenericNumericData";
        setValidationState(s);
        return;
    }

    NodeValidationState s;
    s._state = NodeValidationState::State::Valid;
    setValidationState(s);

    // For now, just switch to time chart view
    m_stack->setCurrentIndex(m_timeChartIndex);
}

QWidget* GenericDisplayNode::embeddedWidget()
{
    return m_stack;
}

void GenericDisplayNode::updateTimeChart(const QVector<QVector<double>>& channels)
{
    // Future: update time domain chart with channel data
    Q_UNUSED(channels);
}

void GenericDisplayNode::updateFFTChart(const QVector<QVector<double>>& channels)
{
    // Future: update FFT spectrum with channel data
    Q_UNUSED(channels);
}
