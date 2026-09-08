#include "GpuMonitorWidget.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

GpuMonitorWidget::GpuMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    m_intervalSpin = new QDoubleSpinBox(this);
    m_intervalSpin->setRange(0.1, 5.0);
    m_intervalSpin->setSingleStep(0.1);
    m_intervalSpin->setDecimals(1);
    m_intervalSpin->setValue(1.0);
    m_intervalSpin->setSuffix(QStringLiteral(" s"));
    m_intervalSpin->setToolTip(QStringLiteral("Polling interval (0.1–5.0 s)"));

    m_startStopButton = new QPushButton(QStringLiteral("Start"), this);
    m_startStopButton->setCheckable(true);

    m_statusLabel = new QLabel(QStringLiteral("GPU Monitor — stopped"), this);
    m_statusLabel->setWordWrap(true);

    auto *intervalRow = new QHBoxLayout;
    intervalRow->addWidget(new QLabel(QStringLiteral("Interval:"), this));
    intervalRow->addWidget(m_intervalSpin);
    intervalRow->addWidget(m_startStopButton);
    intervalRow->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(intervalRow);
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    connect(m_startStopButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked)
            emit startRequested();
        else
            emit stopRequested();
    });
    connect(m_intervalSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GpuMonitorWidget::intervalChanged);
}

void GpuMonitorWidget::updateMetrics(const GpuMonitorEngine::Metrics &m)
{
    setStatusText(QStringLiteral("%1 — %2%% | mem %3%% | %4°C | %5 W | fan %6%% | %7 MHz")
                      .arg(QStringLiteral("GPU"))
                      .arg(m.gpuUtil, 0, 'f', 0)
                      .arg(m.memUsedPct, 0, 'f', 0)
                      .arg(m.tempC, 0, 'f', 0)
                      .arg(m.powerW, 0, 'f', 1)
                      .arg(m.fanPct, 0, 'f', 0)
                      .arg(m.clockMhz, 0, 'f', 0));
}

void GpuMonitorWidget::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

double GpuMonitorWidget::intervalSeconds() const
{
    return m_intervalSpin->value();
}

void GpuMonitorWidget::setIntervalSeconds(double seconds)
{
    m_intervalSpin->setValue(qBound(0.1, seconds, 5.0));
}

void GpuMonitorWidget::setRunning(bool running)
{
    m_startStopButton->setChecked(running);
    m_startStopButton->setText(running ? QStringLiteral("Stop") : QStringLiteral("Start"));
}
