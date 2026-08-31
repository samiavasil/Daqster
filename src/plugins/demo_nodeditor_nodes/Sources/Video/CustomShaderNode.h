#ifndef CUSTOMSHADERNODE_H
#define CUSTOMSHADERNODE_H

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

#include "CustomShaderGLProcessor.h"

#include <QtNodes/NodeDelegateModel>

#include <QElapsedTimer>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>

#include <memory>

class VideoFrameData;

/// Runtime GLSL shader node — Shadertoy-style mainImage contract.
class CustomShaderNode : public QtNodes::NodeDelegateModel
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

    QWidget *embeddedWidget() override;

private:
    void buildWidget();
    void reprocessCurrentFrame();

    std::shared_ptr<VideoFrameData> m_lastInput;
    std::shared_ptr<VideoFrameData> m_output;

    CustomShaderGLProcessor m_processor;

    QWidget *m_widget = nullptr;
    QPlainTextEdit *m_glslEditor = nullptr;
    QPlainTextEdit *m_errorLog = nullptr;
    QPushButton *m_compileButton = nullptr;
    QSlider *m_sliders[4] = {nullptr, nullptr, nullptr, nullptr};
    QLabel *m_sliderLabels[4] = {nullptr, nullptr, nullptr, nullptr};
    QCheckBox *m_animateCheck = nullptr;
    QElapsedTimer m_elapsed;
};

#endif // CUSTOMSHADERNODE_H
