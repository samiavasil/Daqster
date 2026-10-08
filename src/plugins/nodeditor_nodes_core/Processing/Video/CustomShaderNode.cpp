#include "CustomShaderNode.h"
#include <QString>

#include "GL/TexturePool.h"
#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"

#include <QJsonObject>

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
    : m_config{defaultShader(), {0.0f, 0.0f, 0.0f, 0.0f}, false}
{
    m_elapsed.start();
}

CustomShaderNode::~CustomShaderNode() = default;

QJsonObject CustomShaderNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    obj[QStringLiteral("glslSource")] = m_config.source;
    for (int i = 0; i < 4; ++i)
        obj[QStringLiteral("param%1").arg(i)] = int(m_config.param[i] * 100.0f);
    obj[QStringLiteral("animate")] = m_config.animate;
    return obj;
}

void CustomShaderNode::load(QJsonObject const &p)
{
    if (p.contains(QStringLiteral("glslSource")))
        m_config.source = p.value(QStringLiteral("glslSource")).toString();
    for (int i = 0; i < 4; ++i) {
        const QString key = QString("param%1").arg(i);
        if (p.contains(key))
            m_config.param[i] = p.value(key).toInt() / 100.0f;
    }
    if (p.contains(QStringLiteral("animate")))
        m_config.animate = p.value(QStringLiteral("animate")).toBool();
    Q_EMIT configChanged(m_config);
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
        Q_EMIT hardwareGlMissing();
        Q_EMIT dataInvalidated(0);
        return;
    }

    reprocessCurrentFrame();
}

void CustomShaderNode::onCompileRequested()
{
    reprocessCurrentFrame();
}

void CustomShaderNode::onSourceChanged(const QString& source)
{
    m_config.source = source;
    reprocessCurrentFrame();
}

void CustomShaderNode::onParamChanged(int index, int value)
{
    if (index >= 0 && index < 4)
        m_config.param[index] = value / 100.0f;
    reprocessCurrentFrame();
}

void CustomShaderNode::onAnimateToggled(bool animate)
{
    m_config.animate = animate;
    if (animate)
        m_elapsed.restart();
    reprocessCurrentFrame();
}

void CustomShaderNode::reprocessCurrentFrame()
{
    if (!m_lastInput || !m_lastInput->hasFrame())
        return;

    ShaderParams p;
    p.param0 = m_config.param[0];
    p.param1 = m_config.param[1];
    p.param2 = m_config.param[2];
    p.param3 = m_config.param[3];
    p.animate = m_config.animate;
    if (p.animate)
        p.time = m_elapsed.elapsed() / 1000.0f;

    VideoTextureHandle input;
    if (!m_lastInput->asTexture(&input)) {
        Q_EMIT compilationError(QStringLiteral("Failed to get GPU texture from input frame"));
        Q_EMIT dataInvalidated(0);
        return;
    }

    VideoTextureHandle out;
    if (m_processor.processTexture(input, m_config.source, p, &out)) {
        Q_EMIT compilationOk();
        // Texture-pool path (REQ-SW-PL-032 Issue #7 / REQ-SW-PL-038):
        // the output texture is returned to the global pool when the frame
        // dies instead of being deleted.
        m_output = VideoFrameData::fromTexture(
            out, [tex = out.texY]() {
                TexturePool::instance().release(tex);
            });
        Q_EMIT dataUpdated(0);
    } else {
        Q_EMIT compilationError(m_processor.lastErrorLog());
        m_output.reset();
        Q_EMIT dataInvalidated(0);
    }
}
