#pragma once

#include <QtWidgets/QWidget>

class QComboBox;
class QSpinBox;

/**
 * @brief GUI widget of the Frame Sampler node (REQ-SW-PL-051 core/gui split).
 *
 * Owns the mode selector and the two parameter spin boxes. The gating logic
 * stays in the core FrameSamplerNode.
 */
class FrameSamplerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FrameSamplerWidget(QWidget* parent = nullptr);
    ~FrameSamplerWidget() override = default;

public slots:
    /// Model → widget: a parameter changed (or load() restored one).
    void setParams(int mode, int everyN, int maxFps);

signals:
    void modeChanged(int mode);
    void everyNChanged(int everyN);
    void maxFpsChanged(int maxFps);

private:
    QComboBox* m_modeCombo = nullptr;
    QSpinBox* m_everyNSpin = nullptr;
    QSpinBox* m_maxFpsSpin = nullptr;
};