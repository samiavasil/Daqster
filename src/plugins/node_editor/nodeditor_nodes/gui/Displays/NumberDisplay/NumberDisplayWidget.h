#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>

class NumberDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NumberDisplayWidget(QWidget* parent = nullptr);
    ~NumberDisplayWidget() override = default;

    QComboBox* typeCombo() { return m_typeCombo; }
    QLabel* label() { return m_label; }

public slots:
    /// Rendered by the core model via NumberDisplayDataModel::displayTextChanged.
    void setDisplayText(const QString& text);

private:
    QLabel* m_label = nullptr;
    QComboBox* m_typeCombo = nullptr;
};
