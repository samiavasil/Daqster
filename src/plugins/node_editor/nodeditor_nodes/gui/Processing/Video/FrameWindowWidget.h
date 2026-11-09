#pragma once

#include <QtWidgets/QWidget>

class QComboBox;
class QSpinBox;

/**
 * @brief GUI widget of the Frame Window node (REQ-SW-PL-086).
 *
 * Owns the emit-mode selector, the window-size spin box and the interval spin
 * box. The sampling logic stays in the core FrameWindowNode.
 */
class FrameWindowWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FrameWindowWidget(QWidget *parent = nullptr);
    ~FrameWindowWidget() override = default;

public slots:
    /// Model → widget: a parameter changed (or load() restored one).
    void setParams(int mode, int windowSize, int intervalMs);

signals:
    void modeChanged(int mode);
    void windowChanged(int windowSize);
    void intervalMsChanged(int intervalMs);

private:
    QComboBox *m_modeCombo = nullptr;
    QSpinBox *m_windowSpin = nullptr;
    QSpinBox *m_intervalSpin = nullptr;
};
