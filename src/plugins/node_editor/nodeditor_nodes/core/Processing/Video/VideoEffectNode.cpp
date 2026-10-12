#include "VideoEffectNode.h"

#include "GL/TexturePool.h"
#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "PerfProfiler.h"
#include "Threading/ComputePool.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

#include <algorithm>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

using Daqster::ComputePool;

VideoEffectNode::VideoEffectNode()
{
    m_specs = VideoEffectOps::allSpecs();

    // Per-node key into the shared ComputePool (REQ-SW-PL-039): per-key
    // latest-wins submission + serialization for the CPU path.
    m_poolKey = QByteArray::number(reinterpret_cast<quintptr>(this));

    // Optional [PERF] effect console line (5 s timer, mirrors
    // VideoOutputNode::logPerfLine). No-op unless the "video" perf domain is
    // enabled (VideoOutputNode's Perf checkbox).
    m_perfTimer = new QTimer(this);
    m_perfTimer->setInterval(5000);
    connect(m_perfTimer, &QTimer::timeout, this, &VideoEffectNode::logPerfLine);
    m_perfTimer->start();

    // Initial config emit so the widget (when built) gets the default state.
    Q_EMIT configChanged(m_effectIndex, m_params);
}

VideoEffectNode::~VideoEffectNode()
{
    // Single shutdown path: stop() cancels pool tasks + stops the timer
    // (REQ-SW-PL-050).
    stop();
}

void VideoEffectNode::stop()
{
    // Idempotent: setting the flag + cancelling an already-cancelled key is
    // safe to repeat.
    m_shuttingDown = true; // worker tasks check before posting results

    // Cancel queued/pending CPU tasks for this key and wait for the running
    // one (≤ 500 ms) so no worker touches `this` after destruction.
    ComputePool::instance().cancel(m_poolKey);

    if (m_perfTimer)
        m_perfTimer->stop();
}

QJsonObject VideoEffectNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    if (m_effectIndex >= 0 && m_effectIndex < m_specs.size())
        obj[QStringLiteral("effect")] = m_specs[m_effectIndex].id;
    obj[QStringLiteral("brightness")] = m_params.brightness;
    obj[QStringLiteral("contrast")] = m_params.contrast;
    obj[QStringLiteral("flipMode")] = m_params.flipHorizontal
        ? QStringLiteral("horizontal")
        : QStringLiteral("vertical");
    obj[QStringLiteral("blurRadius")] = m_params.blurRadius;
    obj[QStringLiteral("gaussianKernel")] = m_params.gaussianKernel;
    obj[QStringLiteral("cannyLow")] = m_params.cannyLow;
    obj[QStringLiteral("cannyHigh")] = m_params.cannyHigh;
    obj[QStringLiteral("thresholdValue")] = m_params.thresholdValue;
    return obj;
}

void VideoEffectNode::load(QJsonObject const &p)
{
    m_params.brightness = p.value(QStringLiteral("brightness")).toInt(m_params.brightness);
    m_params.contrast = p.value(QStringLiteral("contrast")).toInt(m_params.contrast);
    m_params.flipHorizontal = (p.value(QStringLiteral("flipMode")).toString()
                               != QStringLiteral("vertical"));
    m_params.blurRadius = p.value(QStringLiteral("blurRadius")).toInt(m_params.blurRadius);
    m_params.gaussianKernel = p.value(QStringLiteral("gaussianKernel")).toInt(m_params.gaussianKernel);
    m_params.cannyLow = p.value(QStringLiteral("cannyLow")).toInt(m_params.cannyLow);
    m_params.cannyHigh = p.value(QStringLiteral("cannyHigh")).toInt(m_params.cannyHigh);
    m_params.thresholdValue = p.value(QStringLiteral("thresholdValue")).toInt(m_params.thresholdValue);

    m_params.brightness = std::max(-100, std::min(100, m_params.brightness));
    m_params.contrast = std::max(0, std::min(200, m_params.contrast));
    m_params.blurRadius = std::max(0, std::min(10, m_params.blurRadius));
    m_params.gaussianKernel = std::max(1, std::min(31, m_params.gaussianKernel | 1));
    m_params.cannyLow = std::max(0, std::min(255, m_params.cannyLow));
    m_params.cannyHigh = std::max(0, std::min(255, m_params.cannyHigh));
    m_params.thresholdValue = std::max(0, std::min(255, m_params.thresholdValue));

    // Unknown effect id -> setEffect falls back to index 0 safely.
    setEffect(p.value(QStringLiteral("effect")).toString());
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}

QJsonObject VideoEffectNode::configSchema() const
{
    // Describes exactly the object save() emits. `visibleWhen` is a documented
    // extension: { "<field>": [allowed values] } — the control plane's web
    // console hides fields whose condition is not met.
    auto numberField = [](double minimum, double maximum, double def,
                          const QString& description) {
        QJsonObject field;
        field.insert(QStringLiteral("type"), QStringLiteral("number"));
        field.insert(QStringLiteral("minimum"), minimum);
        field.insert(QStringLiteral("maximum"), maximum);
        field.insert(QStringLiteral("default"), def);
        field.insert(QStringLiteral("description"), description);
        return field;
    };
    auto visibleFor = [](const QStringList& effects) {
        QJsonArray values;
        for (const QString& effect : effects)
            values.append(effect);
        return QJsonObject{{QStringLiteral("effect"), values}};
    };

    QJsonArray effectIds;
    QJsonArray effectNames;
    for (const EffectSpec& spec : m_specs) {
        effectIds.append(spec.id);
        effectNames.append(spec.displayName);
    }

    QJsonObject properties;

    properties.insert(QStringLiteral("effect"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("enum"), effectIds},
        {QStringLiteral("x-enumNames"), effectNames},
        {QStringLiteral("default"), m_specs.isEmpty() ? QString() : m_specs.first().id},
        {QStringLiteral("description"), QStringLiteral("Video effect to apply")}});

    QJsonObject brightness = numberField(-100, 100, 0,
                                         QStringLiteral("Brightness offset (-100..+100)"));
    brightness.insert(QStringLiteral("visibleWhen"), visibleFor({QStringLiteral("brightness")}));
    properties.insert(QStringLiteral("brightness"), brightness);

    QJsonObject contrast = numberField(0, 200, 100,
                                       QStringLiteral("Contrast (100 = unchanged)"));
    contrast.insert(QStringLiteral("visibleWhen"), visibleFor({QStringLiteral("contrast")}));
    properties.insert(QStringLiteral("contrast"), contrast);

    properties.insert(QStringLiteral("flipMode"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("enum"), QJsonArray{QStringLiteral("horizontal"), QStringLiteral("vertical")}},
        {QStringLiteral("default"), QStringLiteral("horizontal")},
        {QStringLiteral("description"), QStringLiteral("Flip direction")},
        {QStringLiteral("visibleWhen"), visibleFor({QStringLiteral("flip")})}});

    QJsonObject blur = numberField(0, 10, 0, QStringLiteral("Box blur radius"));
    blur.insert(QStringLiteral("visibleWhen"), visibleFor({QStringLiteral("blur")}));
    properties.insert(QStringLiteral("blurRadius"), blur);

    QJsonObject gaussian = numberField(1, 31, 5,
                                       QStringLiteral("Gaussian kernel size (odd)"));
    gaussian.insert(QStringLiteral("multipleOf"), 2);
    gaussian.insert(QStringLiteral("visibleWhen"), visibleFor({QStringLiteral("gaussianBlur")}));
    properties.insert(QStringLiteral("gaussianKernel"), gaussian);

    QJsonObject cannyLow = numberField(0, 255, 50, QStringLiteral("Canny low threshold"));
    cannyLow.insert(QStringLiteral("visibleWhen"), visibleFor({QStringLiteral("canny")}));
    properties.insert(QStringLiteral("cannyLow"), cannyLow);

    QJsonObject cannyHigh = numberField(0, 255, 150, QStringLiteral("Canny high threshold"));
    cannyHigh.insert(QStringLiteral("visibleWhen"), visibleFor({QStringLiteral("canny")}));
    properties.insert(QStringLiteral("cannyHigh"), cannyHigh);

    QJsonObject threshold = numberField(0, 255, 128,
                                        QStringLiteral("Binary threshold value"));
    threshold.insert(QStringLiteral("visibleWhen"), visibleFor({QStringLiteral("threshold")}));
    properties.insert(QStringLiteral("thresholdValue"), threshold);

    QJsonObject schema;
    schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    schema.insert(QStringLiteral("properties"), properties);
    return schema;
}

unsigned int VideoEffectNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoEffectNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> VideoEffectNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void VideoEffectNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    m_lastInput = std::dynamic_pointer_cast<VideoFrameData>(data);
    m_output.reset();

    if (!m_lastInput || !m_lastInput->hasFrame()) {
        Q_EMIT dataInvalidated(0);
        return;
    }

    ++m_totalFrames;

    const EffectSpec &spec = m_specs[m_effectIndex];

    // Runtime backend selection (REQ-SW-PL-028 AC 2/3): CPU-only effects always
    // run on the CPU; GpuOrCpu effects run on the GPU only with hardware GL.
    const bool useGpu = (spec.backend == EffectSpec::Backend::GpuOrCpu)
        && VideoGLContextManager::hasHardwareGL();

    if (useGpu) {
        // GPU path UNCHANGED — stays on the GUI thread (GL-bound).
        // GPU-resident transport (REQ-SW-PL-032 AC 5): when the input is
        // already GPU-resident (previous effect output / asTexture cache) the
        // handle is taken directly — no upload, no readback. A CPU frame is
        // uploaded lazily once and cached on the shared VideoFrameData.
        VideoTextureHandle input;
        if (m_lastInput->asTexture(&input)) {
            VideoTextureHandle out;
            if (m_glProcessor.processTexture(input, spec, m_params, &out)) {
                // Texture-pool path (REQ-SW-PL-032 Issue #7 / REQ-SW-PL-038):
                // the output texture is returned to the global pool when the
                // frame dies instead of being deleted — no per-frame
                // glGenTextures/glDeleteTextures.
                m_output = VideoFrameData::fromTexture(
                    out, [tex = out.texY]() {
                        TexturePool::instance().release(tex);
                    });
                Q_EMIT gpuPathUsed();
                Q_EMIT dataUpdated(0);
                return;
            }
            // GPU processing failed (GL error) — CPU fallback below.
        }
        // asTexture failed (unsupported format / GL error) — CPU fallback below.
    }

    // CPU path (REQ-SW-PL-039): snapshot on the GUI thread, compute on the
    // shared ComputePool. The worker NEVER touches the shared VideoFrameData —
    // it converts its own frame copy (or uses the pre-readback QImage).
    QImage preReadback;   // GpuRgba input: readback on the GUI thread (as today)
    QVideoFrame frameCopy; // CPU-resident input: implicit-share copy (cheap)
    if (m_lastInput->isGpuRgba()) {
        preReadback = m_lastInput->asImage();
    } else {
        frameCopy = m_lastInput->frame();
    }

    // Snapshot the effect spec + params by value — the worker must not read
    // node members (the user may change the effect/params concurrently).
    const EffectSpec specCopy = spec;
    const EffectParams paramsCopy = m_params;

    ComputePool::instance().submitLatest(
        m_poolKey,
        [this, specCopy, paramsCopy, frameCopy, preReadback]() {
            if (m_shuttingDown.load())
                return;

            // Convert the worker's OWN frame copy (never the shared
            // VideoFrameData) — or use the GUI-thread pre-readback QImage.
            // frameToImageCpu() is pure CPU (no GL/RHI): Qt6 toImage() would
            // create a GL context on the worker thread (QTBUG-131107) and Qt5
            // image() returns null for NV12/YUV420P.
            QImage source = preReadback;
            if (source.isNull() && frameCopy.isValid()) {
                source = VideoFrameData::frameToImageCpu(frameCopy);
            }
            const QImage transformed = applyCpu(source, specCopy, paramsCopy);

            // Do not post after shutdown has begun — the node may be gone.
            if (m_shuttingDown.load())
                return;
            if (transformed.isNull())
                return;

            QMetaObject::invokeMethod(this, "onCpuResult", Qt::QueuedConnection,
                                      Q_ARG(QImage, transformed));
        });
}



int VideoEffectNode::indexOfEffect(const QString &id) const
{
    for (int i = 0; i < m_specs.size(); ++i) {
        if (m_specs[i].id == id)
            return i;
    }
    return -1;
}

void VideoEffectNode::setEffect(const QString &id)
{
    const int index = indexOfEffect(id);
    setEffectIndex(index < 0 ? 0 : index);
    reprocessCurrentFrame();
}

void VideoEffectNode::setEffectIndex(int index)
{
    if (index < 0 || index >= m_specs.size())
        index = 0;

    if (m_effectIndex == index)
        return;
    m_effectIndex = index;

    Q_EMIT configChanged(m_effectIndex, m_params);
}

void VideoEffectNode::onEffectIndexChanged(int index)
{
    setEffectIndex(index);
    reprocessCurrentFrame();
}

void VideoEffectNode::onBrightnessChanged(int value)
{
    m_params.brightness = std::max(-100, std::min(100, value));
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}

void VideoEffectNode::onContrastChanged(int value)
{
    m_params.contrast = std::max(0, std::min(200, value));
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}

void VideoEffectNode::onFlipChanged(int index)
{
    m_params.flipHorizontal = (index == 0);
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}

void VideoEffectNode::onBlurChanged(int value)
{
    m_params.blurRadius = std::max(0, std::min(10, value));
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}

#ifdef HAVE_OPENCV
void VideoEffectNode::onGaussianChanged(int value)
{
    m_params.gaussianKernel = std::max(1, std::min(31, value | 1));
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}

void VideoEffectNode::onCannyLowChanged(int value)
{
    m_params.cannyLow = std::max(0, std::min(255, value));
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}

void VideoEffectNode::onCannyHighChanged(int value)
{
    m_params.cannyHigh = std::max(0, std::min(255, value));
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}

void VideoEffectNode::onThresholdChanged(int value)
{
    m_params.thresholdValue = std::max(0, std::min(255, value));
    Q_EMIT configChanged(m_effectIndex, m_params);
    reprocessCurrentFrame();
}
#endif

void VideoEffectNode::reprocessCurrentFrame()
{
    if (!m_lastInput || !m_lastInput->hasFrame())
        return;
    setInData(m_lastInput, 0);
}

QImage VideoEffectNode::applyCpu(const QImage &source, const EffectSpec &spec,
                                 const EffectParams &params) const
{
    // Reads ONLY the passed spec/params — never node members — so this is safe
    // to call from a ComputePool worker thread (REQ-SW-PL-039).
    if (!spec.cpuApply)
        return source;
    return spec.cpuApply(source, params);
}

void VideoEffectNode::onCpuResult(QImage result)
{
    // GUI thread (Qt::QueuedConnection from the pool worker).
    if (m_shuttingDown.load())
        return;
    if (result.isNull())
        return;

    m_output = std::make_shared<VideoFrameData>(QVideoFrame(result));
    Q_EMIT cpuPathUsed();
    Q_EMIT dataUpdated(0);
    // Metric label is updated by the widget from pool counters
}

QString VideoEffectNode::metricLabelText() const
{
    // Format the LIVE pool counters for this node's key (REQ-SW-PL-056). The
    // old updateMetricLabel() pushed this into a core-owned QLabel that was
    // only ever read by the friend test — the real UI label in VideoEffectWidget
    // (gui plugin) builds the same text from the cpuPathUsed()/gpuPathUsed()
    // signals. The accessor keeps the format assertable without any widgets.
    const quint64 submitted = ComputePool::instance().submitted(m_poolKey);
    const quint64 completed = ComputePool::instance().completed(m_poolKey);
    const quint64 skipped = ComputePool::instance().skipped(m_poolKey);
    const double fps = ComputePool::instance().fps(m_poolKey);

    return QStringLiteral("CPU %1/%2 · %3 skipped · %4 fps out")
        .arg(completed)
        .arg(submitted)
        .arg(skipped)
        .arg(fps, 0, 'f', 1);
}void VideoEffectNode::logPerfLine()
{
    // Optional [PERF] effect console line — mirrors VideoOutputNode::logPerfLine
    // (qInfo() without a category so it is always visible when Perf is on).
    auto &domain = Daqster::Perf::Domain::get("video");
    if (!domain.enabled())
        return;

    const quint64 submitted = ComputePool::instance().submitted(m_poolKey);
    const quint64 completed = ComputePool::instance().completed(m_poolKey);
    const quint64 skipped = ComputePool::instance().skipped(m_poolKey);
    const double fps = ComputePool::instance().fps(m_poolKey);

    qInfo().noquote()
        << QStringLiteral("[PERF] effect | submitted=%1 | completed=%2 | skipped=%3 | fps=%4 | total=%5")
               .arg(submitted)
               .arg(completed)
               .arg(skipped)
               .arg(fps, 0, 'f', 1)
               .arg(m_totalFrames);
}
