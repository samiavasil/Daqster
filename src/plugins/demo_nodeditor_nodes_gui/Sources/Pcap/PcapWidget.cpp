#include "PcapWidget.h"
#include "PcapEngine.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#ifdef HAVE_PCAP
#include <pcap/pcap.h>
#endif

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

    m_promiscCheck = new QCheckBox(QStringLiteral("Promiscuous mode"), this);
    m_promiscCheck->setChecked(true);
    m_promiscCheck->setToolTip(QStringLiteral("Capture all traffic on the interface"));

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
    formLayout->addRow(m_promiscCheck);
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
    connect(m_promiscCheck, &QCheckBox::toggled,
            this, &PcapWidget::promiscuousChanged);
    connect(m_startButton, &QPushButton::clicked,
            this, &PcapWidget::startRequested);
    connect(m_stopButton, &QPushButton::clicked,
            this, &PcapWidget::stopRequested);
}

void PcapWidget::setEngine(PcapEngine *engine)
{
    m_engine = engine;
    if (m_engine) {
        connect(m_engine, &PcapEngine::packetCaptured,
                this, &PcapWidget::onPacketCaptured);
        connect(m_engine, &PcapEngine::statusChanged,
                this, &PcapWidget::onStatusChanged);
        connect(m_engine, &PcapEngine::errorOccurred,
                this, &PcapWidget::onErrorOccurred);
        connect(m_engine, &PcapEngine::statsUpdated,
                this, &PcapWidget::onStatsUpdated);

        // Populate interface list from libpcap
        #ifdef HAVE_PCAP
        char errbuf[PCAP_ERRBUF_SIZE];
        pcap_if_t *alldevs = nullptr;
        if (pcap_findalldevs(&alldevs, errbuf) == 0) {
            QStringList interfaces;
            for (pcap_if_t *d = alldevs; d; d = d->next) {
                if (d->name)
                    interfaces << QString::fromUtf8(d->name);
            }
            setInterfaces(interfaces);
            pcap_freealldevs(alldevs);
        } else {
            setStatusText(QStringLiteral("pcap_findalldevs failed: %1")
                            .arg(QString::fromUtf8(errbuf)));
        }
        #else
        setInterfaces(QStringList() << "lo" << "eth0" << "wlan0");
        setStatusText(QStringLiteral("libpcap not available (built without HAVE_PCAP)"));
        #endif
    }
}

void PcapWidget::onPacketCaptured(const PcapEngine::Packet &packet)
{
    Q_UNUSED(packet);
    // Could update status with packet info if needed
}

void PcapWidget::onStatusChanged(const QString &status)
{
    setStatusText(status);
}

void PcapWidget::onErrorOccurred(const QString &msg)
{
    setStatusText(msg);
    setRunning(false);
}

void PcapWidget::onStatsUpdated(quint64 captured, quint64 dropped, quint64 ifDropped)
{
    updateStats(captured, dropped, ifDropped);
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
    return m_promiscCheck->isChecked();
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
    m_promiscCheck->setEnabled(!running);
}

void PcapWidget::setFilter(const QString &filter)
{
    m_filterEdit->setText(filter);
}

void PcapWidget::setSnaplen(int snaplen)
{
    m_snaplenSpin->setValue(snaplen);
}

void PcapWidget::setPromiscuous(bool promiscuous)
{
    m_promiscCheck->setChecked(promiscuous);
}