#pragma once

#include <QtWidgets/QWidget>

class QComboBox;
class QLineEdit;
class QCheckBox;
class QSpinBox;

/**
 * GUI widget of the NumberSource node (REQ-SW-PL-051 core/gui split).
 *
 * Replaces the former NumberSourceDataUi (.ui based). All state — the typed
 * number, the random flag and the interval — lives in the core
 * NumberSourceDataModel; this widget only renders and reports edits.
 */
class NumberSourceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NumberSourceWidget(QWidget* parent = nullptr);
    ~NumberSourceWidget() override = default;

public slots:
    void setTypeIndex(int index);
    void setText(const QString& text);
    void setRandomEnabled(bool enabled);
    void setInterval(int interval);
    /// The value line edit is read-only while random mode is on.
    void setTextEditable(bool editable);

signals:
    void typeChanged(int index);
    void textEdited(const QString& text);
    void randomToggled(bool enabled);
    void intervalChanged(int interval);

private:
    QComboBox* m_typeCombo = nullptr;
    QLineEdit* m_lineEdit = nullptr;
    QCheckBox* m_randomEnabled = nullptr;
    QSpinBox* m_intervalSpin = nullptr;
};
