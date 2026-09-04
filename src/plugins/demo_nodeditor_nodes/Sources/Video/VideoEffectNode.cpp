#include "VideoEffectNode.h"

#include "GL/TexturePool.h"
#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "PerfProfiler.h"
#include "Threading/ComputePool.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

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

    buildWidget();

    // Optional [PERF] effect console line (5 s timer, mirrors
    // VideoOutputNode::logPerfLine). No-op unless the "video" perf domain is
    // enabled (VideoOutputNode's Perf checkbox).
    m_perfTimer = new QTimer(this);
    m_perfTimer->setInterval(5000);
    connect(m_perfTimer, &QTimer::timeout, this, &VideoEffectNode::logPerfLine);
    m_perfTimer->start();
}

VideoEffectNode::~VideoEffectNode()
{
    m_shuttingDown = true; // worker tasks check before posting results

    // Cancel queued/pending CPU tasks for this key and wait for the running
    // one (≤ 500 ms) so no worker touches `this` after destruction.
    ComputePool::instance().cancel(m_poolKey);

    if (m_perfTimer)
        m_perfTimer->stop();

    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
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
    syncWidgetsFromParams();
    reprocessCurrentFrame();
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
                if (m_metricLabel)
                    m_metricLabel->setText(tr("GPU (GUI thread)"));
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
            QImage source = preReadback;
            if (source.isNull() && frameCopy.isValid()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                source = frameCopy.toImage();
#else
                source = frameCopy.image();
#endif
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

QWidget *VideoEffectNode::embeddedWidget()
{
    return m_widget;
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
    syncWidgetsFromParams();
    reprocessCurrentFrame();
}

void VideoEffectNode::setEffectIndex(int index)
{
    if (index < 0 || index >= m_specs.size())
        index = 0;

    m_effectIndex = index;

    if (m_effectCombo && m_effectCombo->currentIndex() != index) {
        const QSignalBlocker blocker(m_effectCombo);
        m_effectCombo->setCurrentIndex(index);
    }
    if (m_stack && m_stack->currentIndex() != index)
        m_stack->setCurrentIndex(index);
}

void VideoEffectNode::buildWidget()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    m_effectCombo = new QComboBox(m_widget);
    m_effectCombo->setMinimumWidth(190);
    for (const EffectSpec &spec : m_specs) {
        const QString backendLabel = (spec.backend == EffectSpec::Backend::CpuOnly)
            ? QStringLiteral(" (CPU)")
            : QStringLiteral(" (GPU)");
        m_effectCombo->addItem(spec.displayName + backendLabel);
    }
    layout->addWidget(m_effectCombo);

    m_stack = new QStackedWidget(m_widget);
    // Page order must match allSpecs() order so combo index == stack index:
    // brightness, contrast, grayscale, invert, sepia, channelSwap, flip,
    // blur, gaussianBlur, canny, threshold.
    m_stack->addWidget(createSliderPage(
        m_brightnessSlider, m_brightnessValue, -100, 100,
        m_params.brightness, tr("Brightness (-100..+100)"),
        [this](int value) {
            m_params.brightness = value;
            reprocessCurrentFrame();
        }));
    m_stack->addWidget(createSliderPage(
        m_contrastSlider, m_contrastValue, 0, 200,
        m_params.contrast, tr("Contrast (0..200%, 100% = unchanged)"),
        [this](int value) {
            m_params.contrast = value;
            reprocessCurrentFrame();
        }));
    m_stack->addWidget(createInfoPage(tr("Converts every frame to grayscale.")));
    m_stack->addWidget(createInfoPage(tr("Inverts the colors of every frame.")));
    m_stack->addWidget(createInfoPage(tr("Applies a sepia tone to every frame.")));
    m_stack->addWidget(createInfoPage(
        tr("Swaps the red and blue channels (R<->B) on every frame.")));
    m_stack->addWidget(createFlipPage());
    m_stack->addWidget(createSliderPage(
        m_blurSlider, m_blurValue, 0, 10,
        m_params.blurRadius, tr("Blur radius (0..10)"),
        [this](int value) {
            m_params.blurRadius = value;
            reprocessCurrentFrame();
        }));
#ifdef HAVE_OPENCV
    m_stack->addWidget(createGaussianPage());
    m_stack->addWidget(createCannyPage());
    m_stack->addWidget(createSliderPage(
        m_thresholdSlider, m_thresholdValue, 0, 255,
        m_params.thresholdValue, tr("Threshold value (0..255)"),
        [this](int value) {
            m_params.thresholdValue = value;
            reprocessCurrentFrame();
        }));
#endif
    layout->addWidget(m_stack, 1);

    // CPU metric label (REQ-SW-PL-039): refreshed on each CPU result with the
    // pool's per-key counters; shows "GPU (GUI thread)" for GPU-path results.
    m_metricLabel = new QLabel(QStringLiteral("CPU --"), m_widget);
    m_metricLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_metricLabel);

    connect(m_effectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                setEffectIndex(index);
                reprocessCurrentFrame();
            });

    setEffectIndex(0);
}

QWidget *VideoEffectNode::createInfoPage(const QString &text)
{
    auto *page = new QWidget(m_stack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *label = new QLabel(text, page);
    label->setWordWrap(true);
    layout->addWidget(label);

    return page;
}

QWidget *VideoEffectNode::createSliderPage(QSlider *&sliderOut, QLabel *&valueLabelOut,
                                           int min, int max, int initial, const QString &title,
                                           std::function<void(int)> onChanged)
{
    auto *page = new QWidget(m_stack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *titleLabel = new QLabel(title, page);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto *row = new QHBoxLayout();
    sliderOut = new QSlider(Qt::Horizontal, page);
    sliderOut->setRange(min, max);
    sliderOut->setValue(initial);
    valueLabelOut = new QLabel(QString::number(initial), page);
    valueLabelOut->setMinimumWidth(32);
    valueLabelOut->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(sliderOut, 1);
    row->addWidget(valueLabelOut);
    layout->addLayout(row);

    connect(sliderOut, &QSlider::valueChanged, this,
            [valueLabelOut, onChanged](int value) {
                valueLabelOut->setText(QString::number(value));
                if (onChanged)
                    onChanged(value);
            });

    return page;
}

QWidget *VideoEffectNode::createFlipPage()
{
    auto *page = new QWidget(m_stack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *titleLabel = new QLabel(tr("Flip direction"), page);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    m_flipCombo = new QComboBox(page);
    m_flipCombo->addItem(tr("Horizontal"), true);
    m_flipCombo->addItem(tr("Vertical"), false);
    m_flipCombo->setCurrentIndex(m_params.flipHorizontal ? 0 : 1);
    layout->addWidget(m_flipCombo);

    connect(m_flipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                m_params.flipHorizontal = m_flipCombo->currentData().toBool();
                reprocessCurrentFrame();
            });

    return page;
}

#ifdef HAVE_OPENCV
QWidget *VideoEffectNode::createGaussianPage()
{
    QWidget *page = createSliderPage(m_gaussianSlider, m_gaussianValue, 1, 31,
                                     m_params.gaussianKernel, tr("Kernel size (odd, 1..31)"),
                                     [this](int value) {
                                         m_params.gaussianKernel = value | 1;
                                         reprocessCurrentFrame();
                                     });
    m_gaussianSlider->setSingleStep(2);
    return page;
}

QWidget *VideoEffectNode::createCannyPage()
{
    auto *page = new QWidget(m_stack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *titleLabel = new QLabel(tr("Canny edge detection thresholds"), page);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto *lowRow = new QHBoxLayout();
    m_cannyLowSlider = new QSlider(Qt::Horizontal, page);
    m_cannyLowSlider->setRange(0, 255);
    m_cannyLowSlider->setValue(m_params.cannyLow);
    m_cannyLowValue = new QLabel(QString::number(m_params.cannyLow), page);
    m_cannyLowValue->setMinimumWidth(32);
    m_cannyLowValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lowRow->addWidget(new QLabel(tr("Low"), page));
    lowRow->addWidget(m_cannyLowSlider, 1);
    lowRow->addWidget(m_cannyLowValue);
    layout->addLayout(lowRow);

    auto *highRow = new QHBoxLayout();
    m_cannyHighSlider = new QSlider(Qt::Horizontal, page);
    m_cannyHighSlider->setRange(0, 255);
    m_cannyHighSlider->setValue(m_params.cannyHigh);
    m_cannyHighValue = new QLabel(QString::number(m_params.cannyHigh), page);
    m_cannyHighValue->setMinimumWidth(32);
    m_cannyHighValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    highRow->addWidget(new QLabel(tr("High"), page));
    highRow->addWidget(m_cannyHighSlider, 1);
    highRow->addWidget(m_cannyHighValue);
    layout->addLayout(highRow);

    connect(m_cannyLowSlider, &QSlider::valueChanged, this, [this](int value) {
        m_params.cannyLow = value;
        m_cannyLowValue->setText(QString::number(value));
        reprocessCurrentFrame();
    });
    connect(m_cannyHighSlider, &QSlider::valueChanged, this, [this](int value) {
        m_params.cannyHigh = value;
        m_cannyHighValue->setText(QString::number(value));
        reprocessCurrentFrame();
    });

    return page;
}
#endif // HAVE_OPENCV

void VideoEffectNode::syncWidgetsFromParams()
{
    if (m_brightnessSlider)
        m_brightnessSlider->setValue(m_params.brightness);
    if (m_contrastSlider)
        m_contrastSlider->setValue(m_params.contrast);
    if (m_flipCombo)
        m_flipCombo->setCurrentIndex(m_params.flipHorizontal ? 0 : 1);
    if (m_blurSlider)
        m_blurSlider->setValue(m_params.blurRadius);
#ifdef HAVE_OPENCV
    if (m_gaussianSlider)
        m_gaussianSlider->setValue(m_params.gaussianKernel);
    if (m_cannyLowSlider)
        m_cannyLowSlider->setValue(m_params.cannyLow);
    if (m_cannyHighSlider)
        m_cannyHighSlider->setValue(m_params.cannyHigh);
    if (m_thresholdSlider)
        m_thresholdSlider->setValue(m_params.thresholdValue);
#endif
}

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
    updateMetricLabel();
    Q_EMIT dataUpdated(0);
}

void VideoEffectNode::updateMetricLabel()
{
    if (!m_metricLabel)
        return;

    // Pool per-key counters (REQ-SW-PL-039): completed/submitted/skipped +
    // completed/sec over the rolling 1 s window.
    const quint64 submitted = ComputePool::instance().submitted(m_poolKey);
    const quint64 completed = ComputePool::instance().completed(m_poolKey);
    const quint64 skipped = ComputePool::instance().skipped(m_poolKey);
    const double fps = ComputePool::instance().fps(m_poolKey);

    m_metricLabel->setText(QStringLiteral("CPU %1/%2 · %3 skipped · %4 fps out")
                               .arg(completed)
                               .arg(submitted)
                               .arg(skipped)
                               .arg(fps, 0, 'f', 1));
}

void VideoEffectNode::logPerfLine()
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
