#include "CameraSourceWidget.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

CameraSourceWidget::CameraSourceWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setMinimumWidth(180);
    layout->addWidget(m_deviceCombo);

    auto* buttonRow = new QHBoxLayout();
    m_startStopButton = new QPushButton(tr("Start"), this);
    m_statusLabel = new QLabel(tr("Stopped"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    buttonRow->addWidget(m_startStopButton);
    buttonRow->addWidget(m_statusLabel, 1);
    layout->addLayout(buttonRow);

    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraSourceWidget::deviceIndexChanged);
    connect(m_startStopButton, &QPushButton::clicked,
            this, &CameraSourceWidget::startStopRequested);
}

void CameraSourceWidget::setDevices(const QStringList& descriptions, int selectedIndex)
{
    const QSignalBlocker blocker(m_deviceCombo);
    m_deviceCombo->clear();
    m_deviceCombo->addItems(descriptions);
    if (selectedIndex >= 0 && selectedIndex < m_deviceCombo->count())
        m_deviceCombo->setCurrentIndex(selectedIndex);
}

void CameraSourceWidget::setStatus(const QString& text, bool ok)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok ? QStringLiteral("color: green;")
                                    : QStringLiteral("color: gray;"));
}

void CameraSourceWidget::setRunning(bool running)
{
    m_startStopButton->setText(running ? tr("Stop") : tr("Start"));
}
