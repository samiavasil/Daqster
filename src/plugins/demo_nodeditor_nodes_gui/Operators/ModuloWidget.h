#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QComboBox>

/**
 * GUI widget of the Modulo node (REQ-SW-PL-051 core/gui split).
 *
 * Owns nothing but the type selector; the actual modulo operation lives in the
 * core ModuloModel.
 */
class ModuloWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModuloWidget(QWidget* parent = nullptr);
    ~ModuloWidget() override = default;

    QComboBox* typeCombo() const { return m_typeCombo; }

public slots:
    /// Select the data type by combo index (0 = int, 1 = double).
    void setTypeIndex(int index);

signals:
    /// Emitted when the user picks a different type.
    void typeChanged(int index);

private:
    QComboBox* m_typeCombo = nullptr;
};
