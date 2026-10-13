#pragma once

#include <QtWidgets/QWidget>

class QComboBox;
class QSpinBox;
class QLineEdit;
class QCheckBox;

/**
 * GUI widget of the Arithmetic/Logic node (REQ-SW-PL-051 core/gui split).
 *
 * Pure presentation: every value is a control, the evaluation itself lives in
 * the core ArithmeticLogicModel.
 */
class ArithmeticLogicWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ArithmeticLogicWidget(QWidget* parent = nullptr);
    ~ArithmeticLogicWidget() override = default;

public slots:
    void setTypeIndex(int index);
    void setInputCount(int count);
    void setExpression(const QString& expression);
    void setStrobeEnabled(bool enabled);

signals:
    void typeChanged(int index);
    void inputCountChanged(int count);
    void expressionChanged(const QString& expression);
    void strobeToggled(bool enabled);

private:
    QComboBox* m_typeCombo = nullptr;
    QSpinBox* m_inputSpin = nullptr;
    QLineEdit* m_exprEdit = nullptr;
    QCheckBox* m_strobeCheck = nullptr;
};
