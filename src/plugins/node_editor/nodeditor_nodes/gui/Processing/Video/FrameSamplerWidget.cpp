#include "FrameSamplerWidget.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

FrameSamplerWidget::FrameSamplerWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Every N-th frame"), 0);
    m_modeCombo->addItem(tr("Max FPS"), 1);
    layout->addWidget(m_modeCombo);

    m_everyNSpin = new QSpinBox(this);
    m_everyNSpin->setRange(1, 1000);
    m_everyNSpin->setPrefix(tr("N = "));
    layout->addWidget(m_everyNSpin);

    m_maxFpsSpin = new QSpinBox(this);
    m_maxFpsSpin->setRange(1, 120);
    m_maxFpsSpin->setPrefix(tr("Max FPS = "));
    layout->addWidget(m_maxFpsSpin);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FrameSamplerWidget::modeChanged);
    connect(m_everyNSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FrameSamplerWidget::everyNChanged);
    connect(m_maxFpsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FrameSamplerWidget::maxFpsChanged);
}

void FrameSamplerWidget::setParams(int mode, int everyN, int maxFps)
{
    const QSignalBlocker blockMode(m_modeCombo);
    m_modeCombo->setCurrentIndex(mode);

    const QSignalBlocker blockEveryN(m_everyNSpin);
    m_everyNSpin->setValue(everyN);
    m_everyNSpin->setVisible(mode == 0);

    const QSignalBlocker blockMaxFps(m_maxFpsSpin);
    m_maxFpsSpin->setValue(maxFps);
    m_maxFpsSpin->setVisible(mode == 1);
}