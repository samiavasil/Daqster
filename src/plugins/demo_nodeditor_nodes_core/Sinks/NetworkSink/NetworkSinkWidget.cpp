#include "NetworkSinkWidget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

NetworkSinkWidget::NetworkSinkWidget(QWidget *parent)
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

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setText(QStringLiteral("127.0.0.1"));
    form->addRow(tr("Host"), m_hostEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(5000);
    form->addRow(tr("Port"), m_portSpin);

    layout->addLayout(form);

    m_startStopButton = new QPushButton(tr("Start"), this);
    layout->addWidget(m_startStopButton);

    m_statusLabel = new QLabel(tr("Idle"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_startStopButton, &QPushButton::clicked,
            this, &NetworkSinkWidget::onStartStopClicked);
}

QString NetworkSinkWidget::protocol() const
{
    return m_protocolCombo->currentText();
}

void NetworkSinkWidget::setProtocol(const QString &protocol)
{
    const int idx = m_protocolCombo->findText(protocol);
    if (idx >= 0)
        m_protocolCombo->setCurrentIndex(idx);
}

QString NetworkSinkWidget::host() const
{
    return m_hostEdit->text().trimmed();
}

void NetworkSinkWidget::setHost(const QString &host)
{
    m_hostEdit->setText(host);
}

int NetworkSinkWidget::port() const
{
    return m_portSpin->value();
}

void NetworkSinkWidget::setPort(int port)
{
    m_portSpin->setValue(port);
}

void NetworkSinkWidget::setStatus(const QString &status)
{
    m_statusLabel->setText(status);
}

QString NetworkSinkWidget::status() const
{
    return m_statusLabel->text();
}

void NetworkSinkWidget::onStartStopClicked()
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
