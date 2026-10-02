#pragma once

#include <QtWidgets/QWidget>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;

/**
 * @brief GUI widget of the Custom Shader node (REQ-SW-PL-051 core/gui split).
 *
 * Owns the GLSL editor, compile button, error log, four parameter sliders
 * and the animate checkbox. The shader compilation and GPU processing stay
 * in the core CustomShaderNode.
 */
class CustomShaderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CustomShaderWidget(QWidget* parent = nullptr);
    ~CustomShaderWidget() override = default;

public slots:
    /// Model → widget: the shader source or a uniform changed (or load() restored them).
    void setConfig(const QString& source, const float param[4], bool animate);
    /// Model → widget: GPU hardware not available.
    void onHardwareGlMissing();
    /// Model → widget: compilation error message from the processor.
    void onCompilationError(const QString& error);
    /// Model → widget: compilation succeeded — clear the error log.
    void onCompilationOk();

signals:
    void compileRequested();
    void sourceChanged(const QString& source);
    void paramChanged(int index, int value);
    void animateToggled(bool animate);

private:
    QPlainTextEdit* m_glslEditor = nullptr;
    QPushButton* m_compileButton = nullptr;
    QPlainTextEdit* m_errorLog = nullptr;
    QSlider* m_sliders[4] = {nullptr, nullptr, nullptr, nullptr};
    QLabel* m_sliderLabels[4] = {nullptr, nullptr, nullptr, nullptr};
    QCheckBox* m_animateCheck = nullptr;
};