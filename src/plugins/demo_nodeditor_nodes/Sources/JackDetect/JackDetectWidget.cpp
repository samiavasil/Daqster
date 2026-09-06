#include "JackDetectWidget.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

JackDetectWidget::JackDetectWidget(QWidget *parent)
    : QWidget(parent)
{
    m_intervalSpin = new QDoubleSpinBox(this);
    m_intervalSpin->setRange(0.1, 5.0);
    m_intervalSpin->setSingleStep(0.1);
    m_intervalSpin->setDecimals(1);
    m_intervalSpin->setValue(0.5);
    m_intervalSpin->setSuffix(QStringLiteral(" s"));
    m_intervalSpin->setToolTip(QStringLiteral("Polling interval (0.1–5.0 s)"));

    m_startStopButton = new QPushButton(QStringLiteral("Start"), this);
    m_startStopButton->setCheckable(true);

    m_statusLabel = new QLabel(QStringLiteral("Jack Detect — stopped"), this);
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
            this, &JackDetectWidget::intervalChanged);
}

void JackDetectWidget::setJacks(const QVector<JackDetectEngine::JackState> &jacks)
{
    if (jacks.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("No HDA jack files found"));
        return;
    }

    QStringList parts;
    parts.reserve(jacks.size());
    for (const JackDetectEngine::JackState &jack : jacks) {
        parts << QStringLiteral("%1: %2").arg(
            jack.name, jack.present ? QStringLiteral("Yes") : QStringLiteral("No"));
    }
    m_statusLabel->setText(parts.join(QStringLiteral(" | ")));
}

void JackDetectWidget::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

double JackDetectWidget::intervalSeconds() const
{
    return m_intervalSpin->value();
}

void JackDetectWidget::setIntervalSeconds(double seconds)
{
    m_intervalSpin->setValue(qBound(0.1, seconds, 5.0));
}

void JackDetectWidget::setRunning(bool running)
{
    m_startStopButton->setChecked(running);
    m_startStopButton->setText(running ? QStringLiteral("Stop") : QStringLiteral("Start"));
}