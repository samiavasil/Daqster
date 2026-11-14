#include "FrameWindowWidget.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

FrameWindowWidget::FrameWindowWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Full window"), 0);
    m_modeCombo->addItem(tr("Every frame"), 1);
    m_modeCombo->addItem(tr("Interval"), 2);
    layout->addWidget(m_modeCombo);

    m_windowSpin = new QSpinBox(this);
    m_windowSpin->setRange(2, 32);
    m_windowSpin->setPrefix(tr("Window = "));
    layout->addWidget(m_windowSpin);

    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(1, 60000);
    m_intervalSpin->setSuffix(tr(" ms"));
    m_intervalSpin->setPrefix(tr("Interval = "));
    layout->addWidget(m_intervalSpin);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FrameWindowWidget::modeChanged);
    connect(m_windowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FrameWindowWidget::windowChanged);
    connect(m_intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FrameWindowWidget::intervalMsChanged);
}

void FrameWindowWidget::setParams(int mode, int windowSize, int intervalMs)
{
    const QSignalBlocker blockMode(m_modeCombo);
    m_modeCombo->setCurrentIndex(mode);

    const QSignalBlocker blockWindow(m_windowSpin);
    m_windowSpin->setValue(windowSize);

    const QSignalBlocker blockInterval(m_intervalSpin);
    m_intervalSpin->setValue(intervalMs);
    m_intervalSpin->setVisible(mode == 2);
}
