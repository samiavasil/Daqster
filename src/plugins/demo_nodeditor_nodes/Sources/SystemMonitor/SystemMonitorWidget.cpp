#include "SystemMonitorWidget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SystemMonitorWidget::SystemMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(4);

    m_intervalSpin = new QDoubleSpinBox(this);
    m_intervalSpin->setRange(0.1, 5.0);
    m_intervalSpin->setDecimals(1);
    m_intervalSpin->setSingleStep(0.1);
    m_intervalSpin->setValue(1.0);
    m_intervalSpin->setSuffix(tr(" s"));
    form->addRow(tr("Poll interval"), m_intervalSpin);

    layout->addLayout(form);

    m_cpuCheck = new QCheckBox(tr("CPU usage"), this);
    m_cpuCheck->setChecked(true);
    layout->addWidget(m_cpuCheck);

    m_ramCheck = new QCheckBox(tr("RAM usage"), this);
    m_ramCheck->setChecked(true);
    layout->addWidget(m_ramCheck);

    m_tempCheck = new QCheckBox(tr("CPU temperature"), this);
    m_tempCheck->setChecked(true);
    layout->addWidget(m_tempCheck);

    m_networkCheck = new QCheckBox(tr("Network throughput"), this);
    m_networkCheck->setChecked(true);
    layout->addWidget(m_networkCheck);

    m_startStopButton = new QPushButton(tr("Start"), this);
    layout->addWidget(m_startStopButton);

    m_statusLabel = new QLabel(tr("Idle"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_startStopButton, &QPushButton::clicked,
            this, &SystemMonitorWidget::onStartStopClicked);

    connect(m_intervalSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SystemMonitorWidget::intervalChanged);
    connect(m_cpuCheck, &QCheckBox::toggled, this, [this](bool) {
        emit metricsChanged(m_cpuCheck->isChecked(), m_ramCheck->isChecked(),
                            m_tempCheck->isChecked(), m_networkCheck->isChecked());
    });
    connect(m_ramCheck, &QCheckBox::toggled, this, [this](bool) {
        emit metricsChanged(m_cpuCheck->isChecked(), m_ramCheck->isChecked(),
                            m_tempCheck->isChecked(), m_networkCheck->isChecked());
    });
    connect(m_tempCheck, &QCheckBox::toggled, this, [this](bool) {
        emit metricsChanged(m_cpuCheck->isChecked(), m_ramCheck->isChecked(),
                            m_tempCheck->isChecked(), m_networkCheck->isChecked());
    });
    connect(m_networkCheck, &QCheckBox::toggled, this, [this](bool) {
        emit metricsChanged(m_cpuCheck->isChecked(), m_ramCheck->isChecked(),
                            m_tempCheck->isChecked(), m_networkCheck->isChecked());
    });
}

// ── Config accessors ────────────────────────────────────────────────────────

double SystemMonitorWidget::pollIntervalSec() const
{
    return m_intervalSpin->value();
}

bool SystemMonitorWidget::cpuEnabled() const
{
    return m_cpuCheck->isChecked();
}

bool SystemMonitorWidget::ramEnabled() const
{
    return m_ramCheck->isChecked();
}

bool SystemMonitorWidget::tempEnabled() const
{
    return m_tempCheck->isChecked();
}

bool SystemMonitorWidget::networkEnabled() const
{
    return m_networkCheck->isChecked();
}

void SystemMonitorWidget::setPollIntervalSec(double sec)
{
    m_intervalSpin->setValue(sec);
}

void SystemMonitorWidget::setCpuEnabled(bool enabled)
{
    m_cpuCheck->setChecked(enabled);
}

void SystemMonitorWidget::setRamEnabled(bool enabled)
{
    m_ramCheck->setChecked(enabled);
}

void SystemMonitorWidget::setTempEnabled(bool enabled)
{
    m_tempCheck->setChecked(enabled);
}

void SystemMonitorWidget::setNetworkEnabled(bool enabled)
{
    m_networkCheck->setChecked(enabled);
}

// ── Status / start-stop ─────────────────────────────────────────────────────

void SystemMonitorWidget::setStatus(const QString &status)
{
    m_statusLabel->setText(status);
}

void SystemMonitorWidget::onStartStopClicked()
{
    if (m_started) {
        m_started = false;
        m_startStopButton->setText(tr("Start"));
        emit stopRequested();
    } else {
        m_started = true;
        m_startStopButton->setText(tr("Stop"));
        emit startRequested();
    }
}
