#include "ArithmeticLogicWidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>

ArithmeticLogicWidget::ArithmeticLogicWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Type combo
    QHBoxLayout* typeRow = new QHBoxLayout();
    typeRow->addWidget(new QLabel(QStringLiteral("Type:"), this));
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(QStringLiteral("int"));
    m_typeCombo->addItem(QStringLiteral("double"));
    typeRow->addWidget(m_typeCombo);
    layout->addLayout(typeRow);

    // Input count spin
    QHBoxLayout* inputRow = new QHBoxLayout();
    inputRow->addWidget(new QLabel(QStringLiteral("Inputs:"), this));
    m_inputSpin = new QSpinBox(this);
    m_inputSpin->setRange(2, 8);
    m_inputSpin->setValue(2);
    inputRow->addWidget(m_inputSpin);
    layout->addLayout(inputRow);

    // Expression line edit
    layout->addWidget(new QLabel(QStringLiteral("Expression:"), this));
    m_exprEdit = new QLineEdit(this);
    m_exprEdit->setPlaceholderText(QStringLiteral("e.g. a+b"));
    layout->addWidget(m_exprEdit);

    // Strobe checkbox
    m_strobeCheck = new QCheckBox(QStringLiteral("Strobe"), this);
    layout->addWidget(m_strobeCheck);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ArithmeticLogicWidget::typeChanged);
    connect(m_inputSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ArithmeticLogicWidget::inputCountChanged);
    connect(m_exprEdit, &QLineEdit::textChanged,
            this, &ArithmeticLogicWidget::expressionChanged);
    connect(m_strobeCheck, &QCheckBox::toggled,
            this, &ArithmeticLogicWidget::strobeToggled);
}

void ArithmeticLogicWidget::setTypeIndex(int index)
{
    m_typeCombo->blockSignals(true);
    m_typeCombo->setCurrentIndex(index);
    m_typeCombo->blockSignals(false);
}

void ArithmeticLogicWidget::setInputCount(int count)
{
    m_inputSpin->blockSignals(true);
    m_inputSpin->setValue(count);
    m_inputSpin->blockSignals(false);
}

void ArithmeticLogicWidget::setExpression(const QString& expression)
{
    m_exprEdit->blockSignals(true);
    m_exprEdit->setText(expression);
    m_exprEdit->blockSignals(false);
}

void ArithmeticLogicWidget::setStrobeEnabled(bool enabled)
{
    m_strobeCheck->blockSignals(true);
    m_strobeCheck->setChecked(enabled);
    m_strobeCheck->blockSignals(false);
}
