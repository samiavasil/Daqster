#ifndef CUSTOMSHADERNODE_H
#define CUSTOMSHADERNODE_H

#include "demo_nodeditor_nodes_core_export.h"

// SPDX-License-Identifier: MIT
//
// Runtime GLSL shader node (REQ-SW-PL-029). Accepts VideoFrameData on
// port 0 and emits VideoFrameData on port 0. The user writes a
// Shadertoy-style `void mainImage(out vec4 fragColor, in vec2 fragCoord)`
// function in the embedded GLSL editor; the node compiles it on demand and
// applies it to every incoming frame on the GPU.
//
// GPU-only: requires hardware OpenGL (no CPU fallback).
// The embedded widget is fixed-size (no geometry recomputation on data arrival).
//
// REQ-SW-PL-051: this model owns NO widgets. The GLSL editor, compile button,
// error log, sliders and animate checkbox live in CustomShaderWidget (GUI
// plugin) and are created through NodeWidgetFactory. The shader source and
// parameters are kept here as plain fields and pushed to the widget via
// configChanged().

#include "CustomShaderGLProcessor.h"

#include <QtNodes/NodeDelegateModel>

#include <QElapsedTimer>
#include <QJsonObject>

#include <memory>

class VideoFrameData;

/// Runtime parameters for the shader (shader source + uniforms).
struct ShaderConfig {
    QString source;
    float param[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    bool animate = false;
};

/// Runtime GLSL shader node — Shadertoy-style mainImage contract.
class DEMO_NODEDITOR_NODES_CORE_EXPORT CustomShaderNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    CustomShaderNode();
    ~CustomShaderNode() override;

    QString caption() const override
    { return QStringLiteral("Custom Shader"); }

    bool captionVisible() const override
    { return true; }

    QString name() const override
    { return QStringLiteral("CustomShader"); }

    /// Fixed-size widget — no geometry recomputation on data arrival.
    bool dataArrivalChangesGeometry() const override { return false; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. Opts out of the body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                    QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    /// Core model has no QtWidgets dependency — the widget is created by the
    /// GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Current shader configuration, read by the GUI widget on attach.
    ShaderConfig config() const { return m_config; }

signals:
    /// The shader source or a uniform changed (or load() restored them) —
    /// re-render the editor and controls.
    void configChanged(const ShaderConfig& config);
    /// GPU hardware not available.
    void hardwareGlMissing();
    /// Compilation error message from the processor.
    void compilationError(const QString& error);
    /// Compilation succeeded — clear the error log.
    void compilationOk();

public slots:
    /// "Compile & Apply" button pressed.
    void onCompileRequested();
    /// GLSL editor content changed by the user.
    void onSourceChanged(const QString& source);
    /// Slider `index` changed to `value` (0-100).
    void onParamChanged(int index, int value);
    /// Animate checkbox toggled.
    void onAnimateToggled(bool animate);

private:
    void reprocessCurrentFrame();

    std::shared_ptr<VideoFrameData> m_lastInput;
    std::shared_ptr<VideoFrameData> m_output;

    CustomShaderGLProcessor m_processor;
    ShaderConfig m_config;
    QElapsedTimer m_elapsed;
};

#endif // CUSTOMSHADERNODE_H
