#include "GamepadWidget.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// Button state colors: green = pressed, gray = released (Linux joystick
// convention: value 0 = pressed, 1 = released).
const char *kButtonPressedStyle = "background-color: #2e7d32; color: white;";
const char *kButtonReleasedStyle = "background-color: #9e9e9e; color: white;";

} // namespace

GamepadWidget::GamepadWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(4);

    m_deviceEdit = new QLineEdit(QStringLiteral("/dev/input/js0"), this);
    form->addRow(tr("Device"), m_deviceEdit);

    m_pollRateSpin = new QSpinBox(this);
    m_pollRateSpin->setRange(30, 120);
    m_pollRateSpin->setValue(60);
    m_pollRateSpin->setSuffix(tr(" Hz"));
    form->addRow(tr("Poll rate"), m_pollRateSpin);

    layout->addLayout(form);

    // ── Axis display (4 values) ───────────────────────────────────────────
    auto *axisGrid = new QGridLayout();
    axisGrid->setContentsMargins(0, 0, 0, 0);
    axisGrid->setSpacing(4);
    m_axisXLabel = makeAxisLabel();
    m_axisYLabel = makeAxisLabel();
    m_axisZLabel = makeAxisLabel();
    m_axisRzLabel = makeAxisLabel();
    axisGrid->addWidget(new QLabel(tr("X"), this), 0, 0);
    axisGrid->addWidget(m_axisXLabel, 0, 1);
    axisGrid->addWidget(new QLabel(tr("Y"), this), 0, 2);
    axisGrid->addWidget(m_axisYLabel, 0, 3);
    axisGrid->addWidget(new QLabel(tr("Z"), this), 1, 0);
    axisGrid->addWidget(m_axisZLabel, 1, 1);
    axisGrid->addWidget(new QLabel(tr("Rz"), this), 1, 2);
    axisGrid->addWidget(m_axisRzLabel, 1, 3);
    layout->addLayout(axisGrid);

    // ── Button state (8 indicators) ───────────────────────────────────────
    auto *buttonGrid = new QGridLayout();
    buttonGrid->setContentsMargins(0, 0, 0, 0);
    buttonGrid->setSpacing(4);
    m_buttonALabel = makeButtonLabel(tr("A"));
    m_buttonBLabel = makeButtonLabel(tr("B"));
    m_buttonXLabel = makeButtonLabel(tr("X"));
    m_buttonYLabel = makeButtonLabel(tr("Y"));
    m_buttonLBLabel = makeButtonLabel(tr("LB"));
    m_buttonRBLabel = makeButtonLabel(tr("RB"));
    m_buttonBackLabel = makeButtonLabel(tr("Back"));
    m_buttonStartLabel = makeButtonLabel(tr("Start"));
    buttonGrid->addWidget(m_buttonALabel, 0, 0);
    buttonGrid->addWidget(m_buttonBLabel, 0, 1);
    buttonGrid->addWidget(m_buttonXLabel, 0, 2);
    buttonGrid->addWidget(m_buttonYLabel, 0, 3);
    buttonGrid->addWidget(m_buttonLBLabel, 1, 0);
    buttonGrid->addWidget(m_buttonRBLabel, 1, 1);
    buttonGrid->addWidget(m_buttonBackLabel, 1, 2);
    buttonGrid->addWidget(m_buttonStartLabel, 1, 3);
    layout->addLayout(buttonGrid);

    m_startStopButton = new QPushButton(tr("Start"), this);
    layout->addWidget(m_startStopButton);

    m_statusLabel = new QLabel(tr("Idle"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_startStopButton, &QPushButton::clicked,
            this, &GamepadWidget::onStartStopClicked);

    connect(m_deviceEdit, &QLineEdit::textChanged,
            this, &GamepadWidget::devicePathChanged);
    connect(m_pollRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &GamepadWidget::pollRateChanged);
}

// ── Config accessors ────────────────────────────────────────────────────────

QString GamepadWidget::devicePath() const
{
    return m_deviceEdit->text();
}

int GamepadWidget::pollRateHz() const
{
    return m_pollRateSpin->value();
}

void GamepadWidget::setDevicePath(const QString &path)
{
    m_deviceEdit->setText(path);
}

void GamepadWidget::setPollRateHz(int hz)
{
    m_pollRateSpin->setValue(hz);
}

// ── Status / values ─────────────────────────────────────────────────────────

void GamepadWidget::setStatus(const QString &status)
{
    m_statusLabel->setText(status);
}

void GamepadWidget::setAxisValues(float x, float y, float z, float rz)
{
    m_axisXLabel->setText(QString::number(static_cast<double>(x), 'f', 2));
    m_axisYLabel->setText(QString::number(static_cast<double>(y), 'f', 2));
    m_axisZLabel->setText(QString::number(static_cast<double>(z), 'f', 2));
    m_axisRzLabel->setText(QString::number(static_cast<double>(rz), 'f', 2));
}

void GamepadWidget::setButtonStates(float a, float b, float x, float y,
                                    float lb, float rb, float back, float start)
{
    // Linux joystick convention: 0.0 = pressed, 1.0 = released.
    m_buttonALabel->setStyleSheet(a == 0.0f ? kButtonPressedStyle : kButtonReleasedStyle);
    m_buttonBLabel->setStyleSheet(b == 0.0f ? kButtonPressedStyle : kButtonReleasedStyle);
    m_buttonXLabel->setStyleSheet(x == 0.0f ? kButtonPressedStyle : kButtonReleasedStyle);
    m_buttonYLabel->setStyleSheet(y == 0.0f ? kButtonPressedStyle : kButtonReleasedStyle);
    m_buttonLBLabel->setStyleSheet(lb == 0.0f ? kButtonPressedStyle : kButtonReleasedStyle);
    m_buttonRBLabel->setStyleSheet(rb == 0.0f ? kButtonPressedStyle : kButtonReleasedStyle);
    m_buttonBackLabel->setStyleSheet(back == 0.0f ? kButtonPressedStyle : kButtonReleasedStyle);
    m_buttonStartLabel->setStyleSheet(start == 0.0f ? kButtonPressedStyle : kButtonReleasedStyle);
}

void GamepadWidget::onStartStopClicked()
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

// ── Helpers ─────────────────────────────────────────────────────────────────

QLabel *GamepadWidget::makeAxisLabel()
{
    auto *label = new QLabel(QStringLiteral("0.00"), this);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumWidth(48);
    return label;
}

QLabel *GamepadWidget::makeButtonLabel(const QString &name)
{
    auto *label = new QLabel(name, this);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumWidth(36);
    label->setStyleSheet(kButtonReleasedStyle);
    return label;
}