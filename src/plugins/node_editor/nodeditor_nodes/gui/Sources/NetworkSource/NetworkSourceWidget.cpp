#include "NetworkSourceWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

NetworkSourceWidget::NetworkSourceWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(4);

    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->addItem(QStringLiteral("UDP"));
    m_protocolCombo->addItem(QStringLiteral("TCP"));
    form->addRow(tr("Protocol"), m_protocolCombo);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(5000);
    form->addRow(tr("Port"), m_portSpin);

    m_sampleRateSpin = new QDoubleSpinBox(this);
    m_sampleRateSpin->setRange(1.0, 10000000.0);
    m_sampleRateSpin->setDecimals(1);
    m_sampleRateSpin->setValue(1000.0);
    form->addRow(tr("Sample rate (Hz)"), m_sampleRateSpin);

    m_channelCountSpin = new QSpinBox(this);
    m_channelCountSpin->setRange(1, 16);
    m_channelCountSpin->setValue(2);
    form->addRow(tr("Channels"), m_channelCountSpin);

    m_channelTypeCombo = new QComboBox(this);
    m_channelTypeCombo->addItem(QStringLiteral("INT16"));
    m_channelTypeCombo->addItem(QStringLiteral("FLOAT32"));
    form->addRow(tr("Channel type"), m_channelTypeCombo);

    layout->addLayout(form);

    m_startStopButton = new QPushButton(tr("Start"), this);
    layout->addWidget(m_startStopButton);

    m_statusLabel = new QLabel(tr("Idle"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_startStopButton, &QPushButton::clicked,
            this, &NetworkSourceWidget::onStartStopClicked);
}

QString NetworkSourceWidget::protocol() const
{
    return m_protocolCombo->currentText();
}

void NetworkSourceWidget::setProtocol(const QString &protocol)
{
    const int idx = m_protocolCombo->findText(protocol);
    if (idx >= 0)
        m_protocolCombo->setCurrentIndex(idx);
}

int NetworkSourceWidget::port() const
{
    return m_portSpin->value();
}

void NetworkSourceWidget::setPort(int port)
{
    m_portSpin->setValue(port);
}

double NetworkSourceWidget::sampleRate() const
{
    return m_sampleRateSpin->value();
}

void NetworkSourceWidget::setSampleRate(double rate)
{
    m_sampleRateSpin->setValue(rate);
}

int NetworkSourceWidget::channelCount() const
{
    return m_channelCountSpin->value();
}

void NetworkSourceWidget::setChannelCount(int count)
{
    m_channelCountSpin->setValue(count);
}

QString NetworkSourceWidget::channelType() const
{
    return m_channelTypeCombo->currentText();
}

void NetworkSourceWidget::setChannelType(const QString &type)
{
    const int idx = m_channelTypeCombo->findText(type);
    if (idx >= 0)
        m_channelTypeCombo->setCurrentIndex(idx);
}

void NetworkSourceWidget::setStatus(const QString &status)
{
    m_statusLabel->setText(status);
}

void NetworkSourceWidget::onStartStopClicked()
{
    if (m_running) {
        m_running = false;
        m_startStopButton->setText(tr("Start"));
        emit stopRequested();
    } else {
        m_running = true;
        m_startStopButton->setText(tr("Stop"));
        emit startRequested();
    }
}
