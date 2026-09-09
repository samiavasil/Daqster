#include "VideoOutputNode.h"

#include "GL/TexturePool.h"
#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "PerfProfiler.h"
#include "VideoCompat.h"
#include "VideoDisplayBackend.h"
#include "VideoDisplayWidget.h"
#include "VideoGLBlitWidget.h"
#include "VideoSoftwareWidget.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoOutputNode::VideoOutputNode()
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback (PERF results,
    // tests/performance/performance-video-display-2026-08-13.md).
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    m_widget = new QWidget();

    // Unified display (REQ-SW-PL-053): ONE display widget, backend selected
    // ONCE at construction. Auto-detect: hardware GL → GL blit (GPU), else
    // software (CPU); DAQSTER_VIDEO_BACKEND=gl|software overrides. The display
    // is a child of m_widget — layout-friendly, works embedded (node scene /
    // runtime workspace) and detached (floating window via the nodeeditor
    // deembed mechanism on embeddedWidget()).
    m_display = (detectVideoBackend() == VideoBackend::Gl)
        ? static_cast<VideoDisplayWidget *>(new VideoGLBlitWidget(m_widget))
        : static_cast<VideoDisplayWidget *>(new VideoSoftwareWidget(m_widget));
    m_display->widget()->setMinimumSize(320, 240);

    // Create splitter as main layout
    m_splitter = new QSplitter(Qt::Horizontal, m_widget);
    m_splitter->addWidget(m_display->widget());           // Left: Video (stretch=1)
    m_splitter->setStretchFactor(0, 1);

    // Create controls widget (right pane)
    QWidget* controlsWidget = new QWidget();
    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(4, 4, 4, 4);
    controlsLayout->setSpacing(4);

    // Perf stats panel (simple QFormLayout for now)
    QWidget* perfPanel = new QWidget();
    QFormLayout* perfLayout = new QFormLayout(perfPanel);
    m_fpsLabel = new QLabel("--");
    m_gapLabel = new QLabel("--");
    m_presentLabel = new QLabel("--");
    m_totalLabel = new QLabel("--");
    m_cpuLabel = new QLabel("--");
    m_hwSwLabel = new QLabel("--");
    m_formatLabel = new QLabel("--");
    m_handleLabel = new QLabel("--");
    perfLayout->addRow("FPS:", m_fpsLabel);
    perfLayout->addRow("Gap (ms):", m_gapLabel);
    perfLayout->addRow("Present (ms):", m_presentLabel);
    perfLayout->addRow("Total (ms):", m_totalLabel);
    perfLayout->addRow("CPU (%):", m_cpuLabel);
    perfLayout->addRow("HW/SW:", m_hwSwLabel);
    perfLayout->addRow("Format:", m_formatLabel);
    perfLayout->addRow("Handle:", m_handleLabel);
    controlsLayout->addWidget(perfPanel);

    // Embedded effects (REQ-SW-PL-034): optional, default "No effect" — the
    // zero-copy passthrough is preserved until the user selects an effect.
    buildEffectControls();

    // Move existing effect controls into the controls widget
    controlsLayout->addWidget(m_effectCombo);
    controlsLayout->addWidget(m_effectStack, 1);

    controlsWidget->setMinimumWidth(220);

    // Add to splitter
    m_splitter->addWidget(controlsWidget);
    m_splitter->setCollapsible(1, true);
    m_splitter->setHandleWidth(4);
    m_splitter->setChildrenCollapsible(true);
    // Use a reasonable default width for the video pane (will be adjusted on first show)
    m_splitter->setSizes(QList<int>({640, 220}));

    // Set splitter as m_widget's layout
    QVBoxLayout* mainLayout = new QVBoxLayout(m_widget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_splitter);

    // Perf stats refresh timer: updates the perf labels in the controls panel
    m_perfRefreshTimer = new QTimer(this);
    m_perfRefreshTimer->setInterval(500);
    connect(m_perfRefreshTimer, &QTimer::timeout, this, [this]() {
        auto &domain = Daqster::Perf::Domain::get("video");
        if (!domain.enabled()) {
            m_fpsLabel->setText("--");
            m_gapLabel->setText("--");
            m_presentLabel->setText("--");
            m_totalLabel->setText("--");
            m_cpuLabel->setText("--");
            m_hwSwLabel->setText("--");
            m_formatLabel->setText("--");
            m_handleLabel->setText("--");
            return;
        }

        const double fps = domain.count("source.frame_interval") > 0
            ? 1000.0 / domain.avg("source.frame_interval")
            : 0.0;
        const double gapMs = domain.avg("source.frame_interval");
        const double presentMs = domain.avg("output.present");
        const double totalMs = domain.avg("output.total");
        const double cpuPercent = m_cpu.sample();

        m_fpsLabel->setText(QString::number(fps, 'f', 1));
        m_gapLabel->setText(QString::number(gapMs, 'f', 1));
        m_presentLabel->setText(QString::number(presentMs, 'f', 1));
        m_totalLabel->setText(QString::number(totalMs, 'f', 1));
        m_cpuLabel->setText(QString::number(cpuPercent, 'f', 1));

        // Qt5/Qt6 compatible check for NoHandle
        const int noHandleValue = 0; // QVideoFrame::HandleType::NoHandle == 0 in both Qt5 and Qt6
        const QString hwSw = (m_lastHandleType == noHandleValue) ? "SW" : "HW";
        m_hwSwLabel->setText(hwSw);

        m_formatLabel->setText(QString::number(m_lastPixelFormat));
        m_handleLabel->setText(QString::number(m_lastHandleType));
    });

    // Start perf refresh timer when controls are visible (always visible in this layout)
    m_perfRefreshTimer->start();
}

void VideoOutputNode::buildEffectControls()
{
    m_specs = VideoEffectOps::allSpecs();

    // Effect combo: index 0 = "No effect" (empty id), then one item per effect
    // with a backend suffix mirroring VideoEffectNode (REQ-SW-PL-028 AC 8).
    m_effectCombo = new QComboBox(m_widget);
    m_effectCombo->setMinimumWidth(190);
    m_effectCombo->addItem(tr("No effect"));
    for (const EffectSpec &spec : m_specs) {
        const QString backendLabel = (spec.backend == EffectSpec::Backend::CpuOnly)
            ? QStringLiteral(" (CPU)")
            : QStringLiteral(" (GPU)");
        m_effectCombo->addItem(spec.displayName + backendLabel);
    }

    // Parameter stack: page 0 = blank (no effect), page i+1 = effect i.
    m_effectStack = new QStackedWidget(m_widget);
    m_effectStack->addWidget(new QWidget(m_effectStack)); // blank "No effect" page
    for (int i = 0; i < m_specs.size(); ++i) {
        const EffectSpec &spec = m_specs[i];
        QWidget *page = nullptr;
        if (spec.id == QStringLiteral("brightness")) {
            page = createSliderPage(
                m_params.brightness, -100, 100, tr("Brightness (-100..+100)"),
                [this](int value) { m_params.brightness = value; });
        } else if (spec.id == QStringLiteral("contrast")) {
            page = createSliderPage(
                m_params.contrast, 0, 200, tr("Contrast (0..200%, 100% = unchanged)"),
                [this](int value) { m_params.contrast = value; });
        } else if (spec.id == QStringLiteral("flip")) {
            page = createFlipPage();
        } else if (spec.id == QStringLiteral("blur")) {
            page = createSliderPage(
                m_params.blurRadius, 0, 10, tr("Blur radius (0..10)"),
                [this](int value) { m_params.blurRadius = value; });
        } else if (spec.id == QStringLiteral("gaussianBlur")) {
            page = createSliderPage(
                m_params.gaussianKernel, 1, 31, tr("Gaussian kernel (odd, 1..31)"),
                [this](int value) { m_params.gaussianKernel = value | 1; });
        } else if (spec.id == QStringLiteral("canny")) {
            page = createCannyPage();
        } else if (spec.id == QStringLiteral("threshold")) {
            page = createSliderPage(
                m_params.thresholdValue, 0, 255, tr("Threshold value (0..255)"),
                [this](int value) { m_params.thresholdValue = value; });
        } else {
            // Parameter-less effects (grayscale/invert/sepia/channelSwap):
            // a compact info label describing the effect.
            page = createInfoPage(spec.displayName);
        }
        m_effectStack->addWidget(page);
    }

    connect(m_effectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                setEffectIndex(index);
                // Re-apply the newly selected effect to the last frame so a
                // paused source updates immediately (mirrors
                // VideoEffectNode.cpp:328-332, REQ-SW-PL-039).
                reprocessCurrentFrame();
            });

    // Default: no effect (index 0).
    setEffectIndex(0);
}

void VideoOutputNode::setEffectIndex(int index)
{
    // Index 0 = "No effect" placeholder; indices 1..N map to m_specs[0..N-1].
    if (index <= 0 || index > m_specs.size()) {
        m_effectEnabled = false;
        m_effectIndex = -1;
    } else {
        m_effectEnabled = true;
        m_effectIndex = index - 1;
    }

    if (m_effectCombo != nullptr && m_effectCombo->currentIndex() != index) {
        const QSignalBlocker blocker(m_effectCombo);
        m_effectCombo->setCurrentIndex(index);
    }
    if (m_effectStack != nullptr && m_effectStack->currentIndex() != index)
        m_effectStack->setCurrentIndex(index);
}

QWidget *VideoOutputNode::createSliderPage(int &value, int min, int max,
                                           const QString &title,
                                           std::function<void(int)> onChanged)
{
    auto *page = new QWidget(m_effectStack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *titleLabel = new QLabel(title, page);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto *row = new QHBoxLayout();
    auto *slider = new QSlider(Qt::Horizontal, page);
    slider->setRange(min, max);
    slider->setValue(value);
    auto *valueLabel = new QLabel(QString::number(value), page);
    valueLabel->setMinimumWidth(32);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(slider, 1);
    row->addWidget(valueLabel);
    layout->addLayout(row);

    connect(slider, &QSlider::valueChanged, this,
            [this, &value, valueLabel, onChanged](int v) {
                value = v;
                valueLabel->setText(QString::number(v));
                if (onChanged)
                    onChanged(v);
            });

    return page;
}

QWidget *VideoOutputNode::createFlipPage()
{
    auto *page = new QWidget(m_effectStack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *titleLabel = new QLabel(tr("Flip direction"), page);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto *flipCombo = new QComboBox(page);
    flipCombo->addItem(tr("Horizontal"), true);
    flipCombo->addItem(tr("Vertical"), false);
    flipCombo->setCurrentIndex(m_params.flipHorizontal ? 0 : 1);
    layout->addWidget(flipCombo);

    connect(flipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, flipCombo](int) {
                m_params.flipHorizontal = flipCombo->currentData().toBool();
            });

    return page;
}

QWidget *VideoOutputNode::createCannyPage()
{
    auto *page = new QWidget(m_effectStack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *titleLabel = new QLabel(tr("Canny edge detection thresholds"), page);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto *lowRow = new QHBoxLayout();
    auto *lowSlider = new QSlider(Qt::Horizontal, page);
    lowSlider->setRange(0, 255);
    lowSlider->setValue(m_params.cannyLow);
    auto *lowValue = new QLabel(QString::number(m_params.cannyLow), page);
    lowValue->setMinimumWidth(32);
    lowValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lowRow->addWidget(new QLabel(tr("Low"), page));
    lowRow->addWidget(lowSlider, 1);
    lowRow->addWidget(lowValue);
    layout->addLayout(lowRow);

    auto *highRow = new QHBoxLayout();
    auto *highSlider = new QSlider(Qt::Horizontal, page);
    highSlider->setRange(0, 255);
    highSlider->setValue(m_params.cannyHigh);
    auto *highValue = new QLabel(QString::number(m_params.cannyHigh), page);
    highValue->setMinimumWidth(32);
    highValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    highRow->addWidget(new QLabel(tr("High"), page));
    highRow->addWidget(highSlider, 1);
    highRow->addWidget(highValue);
    layout->addLayout(highRow);

    connect(lowSlider, &QSlider::valueChanged, this, [this, lowValue](int v) {
        m_params.cannyLow = v;
        lowValue->setText(QString::number(v));
    });
    connect(highSlider, &QSlider::valueChanged, this, [this, highValue](int v) {
        m_params.cannyHigh = v;
        highValue->setText(QString::number(v));
    });

    return page;
}

QWidget *VideoOutputNode::createInfoPage(const QString &text)
{
    auto *page = new QWidget(m_effectStack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *label = new QLabel(text, page);
    label->setWordWrap(true);
    layout->addWidget(label);

    return page;
}

VideoOutputNode::~VideoOutputNode()
{
    // Single shutdown path: stop() stops timers (REQ-SW-PL-050).
    stop();

    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

void VideoOutputNode::stop()
{
    // Idempotent: stopping an already-stopped timer is a no-op.
    if (m_perfRefreshTimer != nullptr)
        m_perfRefreshTimer->stop();

    // The display widget is a child of m_widget — no explicit delete
    // (REQ-SW-PL-053). The perf badge is a child of the display widget.
}

QJsonObject VideoOutputNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();

    // Embedded effect (REQ-SW-PL-034): persist the selected effect id only
    // when an effect is active. No-effect saves omit the key — old graphs
    // without "effect" load as no-effect (backward compatible).
    if (m_effectEnabled && m_effectIndex >= 0 && m_effectIndex < m_specs.size())
        obj[QStringLiteral("effect")] = m_specs[m_effectIndex].id;

    // Effect parameters (mirrors VideoEffectNode::save()).
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

    // Splitter state (REQ-SW-PL-053): persist the QSplitter geometry
    if (m_splitter != nullptr) {
        obj[QStringLiteral("splitterState")] = QString::fromLatin1(m_splitter->saveState().toBase64());
    }
    return obj;
}

void VideoOutputNode::load(QJsonObject const &p)
{
    // Effect parameters with defaults (mirrors VideoEffectNode::load()).
    m_params.brightness = p.value(QStringLiteral("brightness")).toInt(m_params.brightness);
    m_params.contrast = p.value(QStringLiteral("contrast")).toInt(m_params.contrast);
    m_params.flipHorizontal = (p.value(QStringLiteral("flipMode")).toString()
                               != QStringLiteral("vertical"));
    m_params.blurRadius = p.value(QStringLiteral("blurRadius")).toInt(m_params.blurRadius);
    m_params.gaussianKernel = p.value(QStringLiteral("gaussianKernel")).toInt(m_params.gaussianKernel);
    m_params.cannyLow = p.value(QStringLiteral("cannyLow")).toInt(m_params.cannyLow);
    m_params.cannyHigh = p.value(QStringLiteral("cannyHigh")).toInt(m_params.cannyHigh);
    m_params.thresholdValue = p.value(QStringLiteral("thresholdValue")).toInt(m_params.thresholdValue);

    // Clamp to valid ranges (mirrors VideoEffectNode.cpp:64-70).
    m_params.brightness = std::max(-100, std::min(100, m_params.brightness));
    m_params.contrast = std::max(0, std::min(200, m_params.contrast));
    m_params.blurRadius = std::max(0, std::min(10, m_params.blurRadius));
    m_params.gaussianKernel = std::max(1, std::min(31, m_params.gaussianKernel | 1));
    m_params.cannyLow = std::max(0, std::min(255, m_params.cannyLow));
    m_params.cannyHigh = std::max(0, std::min(255, m_params.cannyHigh));
    m_params.thresholdValue = std::max(0, std::min(255, m_params.thresholdValue));

    // Effect selection: absent/empty/invalid "effect" id → no effect
    // (backward compatible with old graphs without the key).
    const QString effectId = p.value(QStringLiteral("effect")).toString();
    int comboIndex = 0; // "No effect"
    for (int i = 0; i < m_specs.size(); ++i) {
        if (m_specs[i].id == effectId) {
            comboIndex = i + 1; // combo index = spec index + 1
            break;
        }
    }
    setEffectIndex(comboIndex);

    // Restore splitter state
    if (m_splitter != nullptr) {
        const QString splitterStateStr = p.value(QStringLiteral("splitterState")).toString();
        if (!splitterStateStr.isEmpty()) {
            QByteArray state = QByteArray::fromBase64(splitterStateStr.toLatin1());
            m_splitter->restoreState(state);
        }
    }

    // Re-apply the restored effect/parameters to the current frame (mirrors
    // VideoEffectNode::load()). No-op when no frame has arrived yet or the
    // input edge is not connected (the setInData guard returns early).
    reprocessCurrentFrame();
}

void VideoOutputNode::reprocessCurrentFrame()
{
    if (!m_lastInput || !m_lastInput->hasFrame())
        return;
    setInData(m_lastInput, 0);
}

unsigned int VideoOutputNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
        // Port 0: "video-frame" (zero-copy GPU, single video-frame type).
        return 1;
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoOutputNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portIndex);
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> VideoOutputNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void VideoOutputNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    if (portIndex == 0) {
        // --- Port 0: "video-frame" (zero-copy GPU display path) ---
        // "output.total" spans the whole port-0 frame processing (REQ-SW-PL-027).
        PERF_SCOPE("video", "output.total");

        auto videoFrame = std::dynamic_pointer_cast<VideoFrameData>(data);
        if (videoFrame && videoFrame->hasFrame()) {
            // CRITICAL GUARD: disconnecting the edge does NOT stop the source
            // player — frames keep flowing into setInData and would resurrect
            // the display. The connection flag (not widget nullness) is the
            // correct guard: it is cleared by inputConnectionDeleted(), which
            // also clears the display.
            if (!m_videoInputConnected)
                return;

            // Perf markers for the badge (REQ-SW-PL-027): HW/SW path + pixel
            // format of the frame actually being presented. No-op while the
            // "video" domain is disabled.
            if (PERF_ENABLED("video")) {
                const QVideoFrame &frame = videoFrame->frame();
                m_lastHandleType = static_cast<int>(frame.handleType());
                m_lastPixelFormat = VideoCompat::pixelFormatInt(frame);
            }

            m_lastInput = videoFrame;

            // ── Embedded effect (REQ-SW-PL-034, optional, default none) ─────
            // When no effect is selected (m_effectEnabled == false) this block
            // is SKIPPED ENTIRELY — asTexture()/asImage() are never called and
            // the display path below is byte-identical to a node without
            // embedded effects (zero-copy passthrough preserved).
            if (m_effectEnabled && m_effectIndex >= 0 && m_effectIndex < m_specs.size()) {
                const EffectSpec &spec = m_specs[m_effectIndex];
                // Runtime backend selection (mirrors VideoEffectNode): GpuOrCpu
                // effects run on the GPU only with hardware GL.
                const bool useGpu = (spec.backend == EffectSpec::Backend::GpuOrCpu)
                    && VideoGLContextManager::hasHardwareGL();
                // Tracks whether the GPU path produced the output. When it did
                // NOT (CpuOnly effect, no hardware GL, or a failed
                // asTexture/processTexture) the CPU path below runs — INCLUDING
                // for GpuRgba inputs, whose asImage() readback mirrors
                // VideoEffectNode.cpp:184-188 (REQ-SW-PL-039).
                bool gpuApplied = false;
                if (useGpu) {
                    VideoTextureHandle input;
                    if (videoFrame->asTexture(&input)) {
                        VideoTextureHandle out;
                        if (m_glProcessor.processTexture(input, spec, m_params, &out)) {
                            // Texture-pool path (REQ-SW-PL-032 Issue #7 / REQ-SW-PL-038):
                            // the output texture is returned to the global pool
                            // when the frame dies instead of being deleted.
                            videoFrame = VideoFrameData::fromTexture(
                                out, [tex = out.texY]() {
                                    TexturePool::instance().release(tex);
                                });
                            gpuApplied = true;
                        }
                    }
                }
                // CPU path (or GPU fallback when asTexture/processTexture
                // failed): convert, apply, re-wrap. Runs whenever the GPU path
                // did NOT produce the output — including GpuRgba inputs, which
                // asImage() reads back (mirrors VideoEffectNode.cpp:184-188).
                if (!gpuApplied) {
                    const QImage img = videoFrame->asImage();
                    const QImage transformed = spec.cpuApply ? spec.cpuApply(img, m_params) : img;
                    if (!transformed.isNull())
                        videoFrame = std::make_shared<VideoFrameData>(QVideoFrame(transformed));
                }
            }

            // ── Unified display (REQ-SW-PL-053) ─────────────────────────────
            // ONE display widget, backend fixed at construction. GpuRgba →
            // presentTexture (zero-copy), GpuYuv → presentYuvTexture (zero-copy
            // cached planes), CPU → presentFrame. The software backend falls
            // back to owner->asImage() (readback) inside presentTexture/
            // presentYuvTexture.
            if (m_display != nullptr) {
                PERF_SCOPE("video", "output.present");
                if (videoFrame->isGpuRgba()) {
                    VideoTextureHandle h;
                    if (videoFrame->asTexture(&h))
                        m_display->presentTexture(h, videoFrame);
                    else
                        m_display->presentImage(videoFrame->asImage());
                } else {
                    VideoTextureHandle h;
                    if (videoFrame->asTexture(&h))
                        m_display->presentYuvTexture(h, videoFrame);
                    else
                        m_display->presentFrame(videoFrame->frame());
                }
            }

            // Only emit the output when a downstream processing consumer is
            // connected to the output port.
            if (m_outputConnectionCount > 0) {
                // Zero-copy passthrough (REQ-SW-PL-032): hand the SAME shared
                // VideoFrameData downstream — no QImage readback, no
                // QVideoFrame re-wrap, no duplicate upload. Residency (CPU /
                // GpuYuv / GpuRgba) is preserved for the consumer.
                m_output = videoFrame;
                Q_EMIT dataUpdated(0);
            }
        } else {
            m_lastInput.reset();
            m_output.reset();
            if (m_display != nullptr)
                m_display->clear();
            Q_EMIT dataInvalidated(0);
        }
        return;
    }
}

void VideoOutputNode::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 0)
        ++m_outputConnectionCount;
}

void VideoOutputNode::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 0 && m_outputConnectionCount > 0)
        --m_outputConnectionCount;
}

void VideoOutputNode::inputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.inPortIndex == 0)
        m_videoInputConnected = true;
}

void VideoOutputNode::inputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.inPortIndex != 0)
        return;

    // The port-0 "video-frame" edge was removed. Disconnecting does NOT stop
    // the source player — frames keep flowing into setInData(), so first clear
    // the connection flag that guards the port-0 branch, then reset all video
    // state (mirror of the destructor).
    m_videoInputConnected = false;

    m_lastInput.reset();
    m_output.reset();
    if (m_display != nullptr)
        m_display->clear();
}

QWidget *VideoOutputNode::embeddedWidget()
{
    return m_widget;
}