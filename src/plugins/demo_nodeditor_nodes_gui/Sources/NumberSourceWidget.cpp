#include "NumberSourceWidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtGui/QDoubleValidator>

NumberSourceWidget::NumberSourceWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    // Type selector
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(QStringLiteral("double"));
    m_typeCombo->addItem(QStringLiteral("int"));
    layout->addWidget(m_typeCombo);

    // Value line edit
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText(QStringLiteral("value"));
    m_lineEdit->setValidator(new QDoubleValidator(m_lineEdit));
    m_lineEdit->setMaximumSize(m_lineEdit->sizeHint());
    m_lineEdit->setText(QStringLiteral("0.0"));
    layout->addWidget(m_lineEdit);

    // Random mode
    m_randomEnabled = new QCheckBox(QStringLiteral("Random"), this);
    layout->addWidget(m_randomEnabled);

    QHBoxLayout* intervalRow = new QHBoxLayout();
    intervalRow->addWidget(new QLabel(QStringLiteral("Interval ms:"), this));
    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(50, 10000);
    m_intervalSpin->setSingleStep(50);
    m_intervalSpin->setValue(500);
    intervalRow->addWidget(m_intervalSpin);
    layout->addLayout(intervalRow);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NumberSourceWidget::typeChanged);
    connect(m_lineEdit, &QLineEdit::textChanged,
            this, &NumberSourceWidget::textEdited);
    connect(m_randomEnabled, &QCheckBox::toggled,
            this, &NumberSourceWidget::randomToggled);
    connect(m_intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NumberSourceWidget::intervalChanged);
}

void NumberSourceWidget::setTypeIndex(int index)
{
    m_typeCombo->blockSignals(true);
    m_typeCombo->setCurrentIndex(index);
    m_typeCombo->blockSignals(false);
}

void NumberSourceWidget::setText(const QString& text)
{
    m_lineEdit->blockSignals(true);
    m_lineEdit->setText(text);
    m_lineEdit->blockSignals(false);
}

void NumberSourceWidget::setRandomEnabled(bool enabled)
{
    m_randomEnabled->blockSignals(true);
    m_randomEnabled->setChecked(enabled);
    m_randomEnabled->blockSignals(false);
}

void NumberSourceWidget::setInterval(int interval)
{
    m_intervalSpin->blockSignals(true);
    m_intervalSpin->setValue(interval);
    m_intervalSpin->blockSignals(false);
}

void NumberSourceWidget::setTextEditable(bool editable)
{
    m_lineEdit->setEnabled(editable);
}
