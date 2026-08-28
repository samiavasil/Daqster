#include "CustomShaderNode.h"

#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QVBoxLayout>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// Default placeholder shader: simple grayscale via mainImage.
inline QString defaultShader()
{
    return QStringLiteral(
        "void mainImage(out vec4 fragColor, in vec2 fragCoord) {\n"
        "    vec4 color = texture(u_tex, fragCoord / u_resolution);\n"
        "    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));\n"
        "    fragColor = vec4(vec3(gray), color.a);\n"
        "}\n");
}

} // namespace

CustomShaderNode::CustomShaderNode()
{
    buildWidget();
    m_elapsed.start();
}

CustomShaderNode::~CustomShaderNode()
{
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject CustomShaderNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    obj[QStringLiteral("glslSource")] = m_glslEditor->toPlainText();
    for (int i = 0; i < 4; ++i)
        obj[QStringLiteral("param%1").arg(i)] = m_sliders[i]->value();
    obj[QStringLiteral("animate")] = m_animateCheck->isChecked();
    return obj;
}

void CustomShaderNode::load(QJsonObject const &p)
{
    if (p.contains(QStringLiteral("glslSource")))
        m_glslEditor->setPlainText(p.value(QStringLiteral("glslSource")).toString());
    for (int i = 0; i < 4; ++i) {
        const QString key = QString("param%1").arg(i);
        if (p.contains(key))
            m_sliders[i]->setValue(p.value(key).toInt());
    }
    if (p.contains(QStringLiteral("animate")))
        m_animateCheck->setChecked(p.value(QStringLiteral("animate")).toBool());
    reprocessCurrentFrame();
}

unsigned int CustomShaderNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType CustomShaderNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> CustomShaderNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void CustomShaderNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    m_lastInput = std::dynamic_pointer_cast<VideoFrameData>(data);
    m_output.reset();

    if (!m_lastInput || !m_lastInput->hasFrame()) {
        Q_EMIT dataInvalidated(0);
        return;
    }

    // GPU-only gate: custom shader requires hardware OpenGL.
    if (!VideoGLContextManager::hasHardwareGL()) {
        m_errorLog->setPlainText(QStringLiteral("Custom shader requires hardware OpenGL"));
        Q_EMIT dataInvalidated(0);
        return;
    }

    reprocessCurrentFrame();
}

QWidget *CustomShaderNode::embeddedWidget()
{
    return m_widget;
}

void CustomShaderNode::buildWidget()
{
    m_widget = new QWidget();
    m_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // GLSL editor (monospace, fixed height).
    m_glslEditor = new QPlainTextEdit(m_widget);
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(9);
    m_glslEditor->setFont(monoFont);
    m_glslEditor->setFixedHeight(140);
    m_glslEditor->setPlaceholderText(QStringLiteral(
        "// Shadertoy-style: write void mainImage(out vec4 fragColor, in vec2 fragCoord)\n"
        "// Available uniforms: u_tex, u_resolution, u_time, u_param0..3"));
    m_glslEditor->setPlainText(defaultShader());
    layout->addWidget(m_glslEditor);

    // Compile button.
    m_compileButton = new QPushButton(tr("Compile & Apply"), m_widget);
    layout->addWidget(m_compileButton);
    connect(m_compileButton, &QPushButton::clicked, this, [this]() {
        reprocessCurrentFrame();
    });

    // Error log (read-only, fixed height).
    m_errorLog = new QPlainTextEdit(m_widget);
    m_errorLog->setFixedHeight(70);
    m_errorLog->setReadOnly(true);
    QFont errorFont(QStringLiteral("Monospace"));
    errorFont.setStyleHint(QFont::TypeWriter);
    errorFont.setPointSize(8);
    m_errorLog->setFont(errorFont);
    m_errorLog->setPlaceholderText(tr("Shader compilation log..."));
    layout->addWidget(m_errorLog);

    // 4x slider rows: QSlider (0-100) + QLabel ("u_param0: 0.00").
    const QStringList paramNames = {
        QStringLiteral("u_param0"), QStringLiteral("u_param1"),
        QStringLiteral("u_param2"), QStringLiteral("u_param3")};
    for (int i = 0; i < 4; ++i) {
        auto *row = new QHBoxLayout();
        m_sliders[i] = new QSlider(Qt::Horizontal, m_widget);
        m_sliders[i]->setRange(0, 100);
        m_sliders[i]->setValue(0);
        m_sliderLabels[i] = new QLabel(
            QStringLiteral("%1: 0.00").arg(paramNames[i]), m_widget);
        m_sliderLabels[i]->setMinimumWidth(90);
        row->addWidget(m_sliders[i], 1);
        row->addWidget(m_sliderLabels[i]);
        layout->addLayout(row);

        connect(m_sliders[i], &QSlider::valueChanged, this,
            [this, i, name = paramNames[i]](int value) {
                m_sliderLabels[i]->setText(
                    QStringLiteral("%1: %2").arg(name).arg(value / 100.0f, 0, 'f', 2));
                reprocessCurrentFrame();
            });
    }

    // Animate checkbox.
    m_animateCheck = new QCheckBox(tr("Animate (u_time)"), m_widget);
    layout->addWidget(m_animateCheck);
    connect(m_animateCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked)
            m_elapsed.restart();
        reprocessCurrentFrame();
    });
}

void CustomShaderNode::reprocessCurrentFrame()
{
    if (!m_lastInput || !m_lastInput->hasFrame())
        return;

    ShaderParams p;
    p.param0 = m_sliders[0]->value() / 100.0f;
    p.param1 = m_sliders[1]->value() / 100.0f;
    p.param2 = m_sliders[2]->value() / 100.0f;
    p.param3 = m_sliders[3]->value() / 100.0f;
    p.animate = m_animateCheck->isChecked();
    if (p.animate)
        p.time = m_elapsed.elapsed() / 1000.0f;

    VideoTextureHandle input;
    if (!m_lastInput->asTexture(&input)) {
        m_errorLog->setPlainText(QStringLiteral("Failed to get GPU texture from input frame"));
        Q_EMIT dataInvalidated(0);
        return;
    }

    VideoTextureHandle out;
    if (m_processor.processTexture(input, m_glslEditor->toPlainText(), p, &out)) {
        m_errorLog->clear();
        // Texture-pool path (REQ-SW-PL-032 Issue #7): the output texture is
        // returned to the pool when the frame dies instead of being deleted.
        m_output = VideoFrameData::fromTexture(
            out, [pool = m_processor.texturePool(), tex = out.texY]() {
                pool->release(tex);
            });
        Q_EMIT dataUpdated(0);
    } else {
        m_errorLog->setPlainText(m_processor.lastErrorLog());
        m_output.reset();
        Q_EMIT dataInvalidated(0);
    }
}
