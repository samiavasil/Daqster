#include "PcapWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

PcapWidget::PcapWidget(QWidget *parent)
    : QWidget(parent)
{
    m_interfaceCombo = new QComboBox(this);
    m_interfaceCombo->setToolTip(QStringLiteral("Network interface to capture on"));

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(QStringLiteral("BPF filter (e.g. \"tcp port 80\")"));
    m_filterEdit->setToolTip(QStringLiteral("Berkeley Packet Filter expression"));

    m_snaplenSpin = new QSpinBox(this);
    m_snaplenSpin->setRange(68, 262144);
    m_snaplenSpin->setValue(65535);
    m_snaplenSpin->setToolTip(QStringLiteral("Maximum bytes to capture per packet"));

    auto *promiscCheck = new QCheckBox(QStringLiteral("Promiscuous mode"), this);
    promiscCheck->setChecked(true);
    promiscCheck->setToolTip(QStringLiteral("Capture all traffic on the interface"));

    m_startButton = new QPushButton(QStringLiteral("Start"), this);
    m_stopButton = new QPushButton(QStringLiteral("Stop"), this);
    m_stopButton->setEnabled(false);

    m_statusLabel = new QLabel(QStringLiteral("pcap capture — stopped"), this);
    m_statusLabel->setWordWrap(true);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(m_startButton);
    buttonRow->addWidget(m_stopButton);
    buttonRow->addStretch();

    auto *formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("Interface:"), m_interfaceCombo);
    formLayout->addRow(QStringLiteral("BPF filter:"), m_filterEdit);
    formLayout->addRow(QStringLiteral("Snaplen:"), m_snaplenSpin);
    formLayout->addRow(promiscCheck);
    formLayout->addRow(buttonRow);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    connect(m_interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit interfaceChanged(m_interfaceCombo->currentText()); });
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &PcapWidget::filterChanged);
    connect(m_snaplenSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PcapWidget::snaplenChanged);
    connect(promiscCheck, &QCheckBox::toggled,
            this, &PcapWidget::promiscuousChanged);
    connect(m_startButton, &QPushButton::clicked,
            this, &PcapWidget::startRequested);
    connect(m_stopButton, &QPushButton::clicked,
            this, &PcapWidget::stopRequested);
}

void PcapWidget::setInterfaces(const QStringList &interfaces)
{
    m_interfaceCombo->clear();
    m_interfaceCombo->addItems(interfaces);
}

QString PcapWidget::interface() const
{
    return m_interfaceCombo->currentText();
}

QString PcapWidget::filter() const
{
    return m_filterEdit->text();
}

int PcapWidget::snaplen() const
{
    return m_snaplenSpin->value();
}

bool PcapWidget::promiscuous() const
{
    // Find the checkbox in the form layout
    for (auto *w : findChildren<QCheckBox *>()) {
        if (w->text() == QStringLiteral("Promiscuous mode"))
            return w->isChecked();
    }
    return true;
}

void PcapWidget::updateStats(quint64 captured, quint64 dropped, quint64 ifDropped)
{
    setStatusText(QStringLiteral("Captured: %1 | Kernel drops: %2 | Interface drops: %3")
                      .arg(captured).arg(dropped).arg(ifDropped));
}

void PcapWidget::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

void PcapWidget::setRunning(bool running)
{
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_interfaceCombo->setEnabled(!running);
    m_filterEdit->setEnabled(!running);
    m_snaplenSpin->setEnabled(!running);
}