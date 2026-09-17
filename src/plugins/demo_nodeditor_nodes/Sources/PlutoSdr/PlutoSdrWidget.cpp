#include "PlutoSdrWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

PlutoSdrWidget::PlutoSdrWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(4);

    m_uriEdit = new QLineEdit(QStringLiteral("ip:192.168.2.1"), this);
    m_uriEdit->setPlaceholderText(tr("URI (ip:192.168.2.1 or usb:)"));
    form->addRow(tr("URI"), m_uriEdit);

    m_freqSpin = new QDoubleSpinBox(this);
    m_freqSpin->setRange(70.0, 6000.0);
    m_freqSpin->setDecimals(3);
    m_freqSpin->setValue(98.5);
    m_freqSpin->setSuffix(tr(" MHz"));
    form->addRow(tr("Frequency"), m_freqSpin);

    m_rateSpin = new QDoubleSpinBox(this);
    m_rateSpin->setRange(0.2, 7.5);
    m_rateSpin->setDecimals(3);
    m_rateSpin->setValue(2.4);
    m_rateSpin->setSuffix(tr(" MSPS"));
    form->addRow(tr("Sample rate"), m_rateSpin);

    m_gainModeCombo = new QComboBox(this);
    m_gainModeCombo->addItem(tr("Manual"), QStringLiteral("manual"));
    m_gainModeCombo->addItem(tr("Fast attack"), QStringLiteral("fast_attack"));
    m_gainModeCombo->addItem(tr("Slow attack"), QStringLiteral("slow_attack"));
    form->addRow(tr("Gain mode"), m_gainModeCombo);

    m_gainSpin = new QDoubleSpinBox(this);
    m_gainSpin->setRange(-10.0, 70.0);
    m_gainSpin->setDecimals(1);
    m_gainSpin->setValue(30.0);
    m_gainSpin->setSuffix(tr(" dB"));
    form->addRow(tr("Gain"), m_gainSpin);

    layout->addLayout(form);

    m_startStopButton = new QPushButton(tr("Start"), this);
    layout->addWidget(m_startStopButton);

    m_statusLabel = new QLabel(tr("idle"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_startStopButton, &QPushButton::clicked,
            this, &PlutoSdrWidget::onStartStopClicked);

    // Any config change is forwarded to the model → engine (applied on next start).
    connect(m_uriEdit, &QLineEdit::textChanged, this, &PlutoSdrWidget::configChanged);
    connect(m_freqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlutoSdrWidget::configChanged);
    connect(m_rateSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlutoSdrWidget::configChanged);
    connect(m_gainModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlutoSdrWidget::configChanged);
    connect(m_gainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlutoSdrWidget::configChanged);
}

// ── Config accessors ────────────────────────────────────────────────────────

QString PlutoSdrWidget::uri() const
{
    return m_uriEdit->text();
}

double PlutoSdrWidget::frequencyMhz() const
{
    return m_freqSpin->value();
}

double PlutoSdrWidget::sampleRateMsps() const
{
    return m_rateSpin->value();
}

QString PlutoSdrWidget::gainMode() const
{
    return m_gainModeCombo->currentData().toString();
}

double PlutoSdrWidget::gainDb() const
{
    return m_gainSpin->value();
}

void PlutoSdrWidget::setUri(const QString &uri)
{
    m_uriEdit->setText(uri);
}

void PlutoSdrWidget::setFrequencyMhz(double mhz)
{
    m_freqSpin->setValue(mhz);
}

void PlutoSdrWidget::setSampleRateMsps(double msps)
{
    m_rateSpin->setValue(msps);
}

void PlutoSdrWidget::setGainMode(const QString &mode)
{
    const int idx = m_gainModeCombo->findData(mode);
    if (idx >= 0)
        m_gainModeCombo->setCurrentIndex(idx);
}

void PlutoSdrWidget::setGainDb(double db)
{
    m_gainSpin->setValue(db);
}

// ── Status / start-stop ─────────────────────────────────────────────────────

void PlutoSdrWidget::setStatus(const QString &status)
{
    m_statusLabel->setText(status);
}

void PlutoSdrWidget::onStartStopClicked()
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