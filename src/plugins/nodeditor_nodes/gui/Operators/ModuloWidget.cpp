#include "ModuloWidget.h"

#include <QtWidgets/QVBoxLayout>

ModuloWidget::ModuloWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(QStringLiteral("int"));
    m_typeCombo->addItem(QStringLiteral("double"));
    layout->addWidget(m_typeCombo);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModuloWidget::typeChanged);
}

void ModuloWidget::setTypeIndex(int index)
{
    // Blocked so a programmatic sync (load() / factory init) does not loop
    // back into the model as a user edit.
    m_typeCombo->blockSignals(true);
    m_typeCombo->setCurrentIndex(index);
    m_typeCombo->blockSignals(false);
}
