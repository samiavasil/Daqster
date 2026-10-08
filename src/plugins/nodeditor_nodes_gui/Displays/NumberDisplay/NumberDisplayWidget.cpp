#include "NumberDisplayWidget.h"

NumberDisplayWidget::NumberDisplayWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("double");
    m_typeCombo->addItem("int");
    layout->addWidget(m_typeCombo);

    m_label = new QLabel();
    m_label->setMargin(3);
    layout->addWidget(m_label);
}

void NumberDisplayWidget::setDisplayText(const QString& text)
{
    m_label->setText(text);
    m_label->adjustSize();
}
