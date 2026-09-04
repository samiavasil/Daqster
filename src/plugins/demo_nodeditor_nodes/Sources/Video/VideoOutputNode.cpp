#include "VideoOutputNode.h"

#include "GL/TexturePool.h"
#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "PerfProfiler.h"
#include "VideoCompat.h"
#include "VideoGLBlitWidget.h"
#include "VideoPerfBadge.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QStackedWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QApplication>
#include <QGraphicsVideoItem>
#include <QVideoWidget>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/internal/NodeGraphicsObject.hpp>
#endif

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// Initial GL blit display preference (applied at startup only).
///
/// The `DAQSTER_GL_BLIT` environment variable is a DEBUG override: "0" forces
/// the software display path (also on Qt5), "1" forces the GL blit path (also
/// on Qt6). Unset → per-Qt default:
///   - Qt5: GL blit ON (fastest measured display path, ~15% vs ~34% CPU)
///   - Qt6: native QVideoWidget ON (GL blit OFF)
/// The VALUE matters — the old presence check (qEnvironmentVariableIsSet)
/// treated even "=0" as enabled. After startup the "GPU display" checkbox in
/// the node has the final word (see VideoOutputNode::m_glEnabled).
bool glBlitStartupEnabled()
{
    if (qEnvironmentVariableIsSet("DAQSTER_GL_BLIT"))
        return qEnvironmentVariableIntValue("DAQSTER_GL_BLIT") != 0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return false;
#else
    return true;
#endif
}

/// Probe whether the platform can create a GL context at all. Called BEFORE
/// constructing the QOpenGLWidget so we never create it when GL is missing
/// (VM / remote / software rendering without GL). A real context + surface
/// round-trip is the most reliable signal: QOpenGLContext::create() alone can
/// succeed even when the display cannot be used.
bool glPlatformAvailable()
{
    QOpenGLContext probe;
    QOffscreenSurface surface;
    surface.setFormat(probe.format());
    surface.create();
    if (!probe.create())
        return false;
    if (!surface.isValid())
        return false;
    const bool ok = probe.makeCurrent(&surface);
    if (ok)
        probe.doneCurrent();
    return ok;
}

} // namespace

VideoOutputNode::VideoOutputNode()
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback (PERF results,
    // tests/performance/performance-video-display-2026-08-13.md).
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    m_widget = new QWidget();
    m_layout = new QVBoxLayout(m_widget);
    m_layout->setContentsMargins(4, 4, 4, 4);

    m_label = new QLabel(m_widget);
    m_label->setMinimumSize(320, 240);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setText(tr("No video input"));
    m_label->setStyleSheet(QStringLiteral("background-color: black; color: gray;"));
    m_layout->addWidget(m_label);

    // Perf toggle + console line (REQ-SW-PL-027, both Qt5 + Qt6): enables the
    // "video" profiling domain live and drives the 5 s console timer. On Qt6 it
    // also drives the on-screen badge refresh timer (500 ms); Qt5 keeps the
    // QImage path with no overlay but still logs the copy-paste-able line.
    m_perfCheck = new QCheckBox(tr("Perf"), m_widget);
    m_layout->addWidget(m_perfCheck);

    // "GPU display" toggle (REQ-SW-PL-021): visible + checked by default on
    // BOTH Qt versions — checked = detached display window, unchecked = video
    // renders inside the node (Qt6: in-scene QGraphicsVideoItem; Qt5:
    // software QLabel path). The checkbox drives m_detachedEnabled; m_glEnabled
    // selects the detached backend (Qt5: GL blit when checked; Qt6:
    // DAQSTER_GL_BLIT=1 at startup forces the GL blit widget, otherwise the
    // native QVideoWidget). The env var only picks the initial state — the UI
    // has the final word after startup.
    m_glCheck = new QCheckBox(tr("GPU display"), m_widget);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_glCheck->setChecked(true);              // detached QVideoWidget default
    m_detachedEnabled = true;
    m_glEnabled = glBlitStartupEnabled();     // DAQSTER_GL_BLIT=1 → GL blit
#else
    m_glCheck->setChecked(glBlitStartupEnabled());
    m_detachedEnabled = m_glCheck->isChecked();
    m_glEnabled = m_detachedEnabled;          // checked = GL blit
#endif
    m_layout->addWidget(m_glCheck);
    connect(m_glCheck, &QCheckBox::toggled, this, &VideoOutputNode::setGlEnabled);

    // Embedded effects (REQ-SW-PL-034): optional, default "No effect" — the
    // zero-copy passthrough is preserved until the user selects an effect.
    buildEffectControls();

    m_consoleTimer = new QTimer(this);
    m_consoleTimer->setInterval(5000);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_perfTimer = new QTimer(this);
    m_perfTimer->setInterval(500);
#endif

    connect(m_perfCheck, &QCheckBox::toggled, this, [this](bool checked) {
        Daqster::Perf::Domain::get("video").setEnabled(checked);
        if (checked) {
            m_consoleTimer->start();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            m_perfTimer->start();
#endif
        } else {
            m_consoleTimer->stop();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            m_perfTimer->stop();
            if (m_perfBadge != nullptr)
                m_perfBadge->hide();
#endif
        }
    });
    connect(m_consoleTimer, &QTimer::timeout, this, &VideoOutputNode::logPerfLine);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_perfTimer, &QTimer::timeout, this, &VideoOutputNode::updatePerfBadge);
#endif

    m_label->installEventFilter(this);
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
    m_layout->addWidget(m_effectCombo);

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
    m_layout->addWidget(m_effectStack, 1);

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
    if (m_consoleTimer != nullptr)
        m_consoleTimer->stop();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_perfTimer != nullptr)
        m_perfTimer->stop();
#endif

    // GL blit window (both Qt5 + Qt6) — destroyed like the video widget.
    if (m_glWidget != nullptr) {
        m_glWidget->hide();
        m_glWidget->deleteLater();
        m_glWidget = nullptr;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_videoWidget != nullptr) {
        m_videoWidget->hide();
        m_videoWidget->deleteLater();
        m_videoWidget = nullptr;
    }
    // In-scene QGraphicsVideoItem (REQ-SW-PL-021): child of the node's
    // NodeGraphicsObject — delete it so it stops receiving frames and is
    // removed from the scene with the node.
    if (m_sceneVideoItem != nullptr) {
        m_sceneVideoItem->deleteLater();
        m_sceneVideoItem = nullptr;
    }
    // The badge is a top-level window (not a child of m_videoWidget), so it
    // must be closed explicitly to avoid a dangling overlay window.
    if (m_perfBadge != nullptr) {
        m_perfBadge->hide();
        m_perfBadge->deleteLater();
        m_perfBadge = nullptr;
    }
#endif
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
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
            // the detached display popup. The connection flag (not widget
            // nullness) is the correct guard: it is cleared by
            // inputConnectionDeleted(), which also closes the popup.
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

            // Lazily create the detached display on the first frame.
            // Stage 2C (REQ-SW-PL-032): GpuRgba frames (effect outputs) must
            // use the GL blit widget on Qt6 too — zero-copy presentTexture,
            // no readback, no per-sink RHI upload. CPU/NV12 frames keep the
            // per-Qt default backend (native QVideoWidget on Qt6, GL blit on
            // Qt5).
            const bool wantGlBlit = m_detachedEnabled && !m_glFailed
                && (m_glEnabled
                    || (videoFrame->isGpuRgba()
                        && VideoGLContextManager::hasHardwareGL()));
            ensureVideoWidget(wantGlBlit);

            // Defensive: the widget may be null if the connection was just
            // removed while a frame was in flight.
            if (m_glWidget != nullptr) {
                PERF_SCOPE("video", "output.present");
                if (videoFrame->isGpuRgba()) {
                    // GPU-resident RGBA frame (effect output): present the
                    // texture directly — zero-copy, no upload/readback
                    // (REQ-SW-PL-032 AC 5).
                    VideoTextureHandle h;
                    if (videoFrame->asTexture(&h))
                        m_glWidget->presentTexture(h, videoFrame);
                    else
                        m_glWidget->presentImage(videoFrame->asImage());
                } else {
                    // CPU / GpuYuv frame: present the cached YUV textures
                    // (asTexture) directly — no duplicate upload
                    // (REQ-SW-PL-032). Falls back to the CPU frame path when
                    // the frame is not NV12/YUV420P or GL is unavailable.
                    VideoTextureHandle h;
                    if (videoFrame->asTexture(&h))
                        m_glWidget->presentYuvTexture(h, videoFrame);
                    else
                        m_glWidget->presentFrame(videoFrame->frame());
                }
            }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            else if (m_videoWidget != nullptr) {
                // GPU path: HW buffer → RHI texture → screen (no QImage copy).
                // "output.present" measures the blit (presentFrame).
                PERF_SCOPE("video", "output.present");
                VideoCompat::presentFrame(m_videoWidget->videoSink(),
                                          presentableFrame(videoFrame));
            }
#endif
            else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                // In-scene GPU display (REQ-SW-PL-021, Qt6): "GPU display"
                // checkbox OFF → present the frame on the in-scene
                // QGraphicsVideoItem child of the node's NodeGraphicsObject.
                // GPU path: HW buffer → RHI texture → scene (no QImage copy).
                // "output.present" measures the present (presentFrame).
                if (!m_detachedEnabled) {
                    ensureSceneVideoItem();
                    if (m_sceneVideoItem != nullptr) {
                        PERF_SCOPE("video", "output.present");
                        VideoCompat::presentFrame(m_sceneVideoItem->videoSink(),
                                                  presentableFrame(videoFrame));
                        return;
                    }
                }
#endif
                // Software display path (Qt5 without GL blit / checkbox off /
                // GL unavailable / Qt6 when the in-scene item could not be
                // created): convert the frame and show it in the embedded
                // label — video keeps displaying at the source rate
                // (REQ-SW-PL-021 auto-fallback). Reuse the lazy QImage cache
                // on the shared VideoFrameData (REQ-SW-PL-032) — the
                // conversion happens at most once per frame.
                const QImage image = videoFrame->asImage();
                if (image.isNull())
                    return;
                m_image = image;
                updateDisplay();
            }

            // Only emit the output when a downstream processing consumer is
            // connected to the output port. Otherwise the GPU path handles
            // display in the detached window and the in-node QLabel shows a
            // static placeholder (no per-frame work).
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
            m_image = QImage();
            m_output.reset();
            updateDisplay();
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
    if (conId.inPortIndex == 0) {
        m_videoInputConnected = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // Capture our own NodeId (REQ-SW-PL-021): needed to find the node's
        // NodeGraphicsObject when creating the in-scene QGraphicsVideoItem.
        m_selfNodeId = conId.inNodeId;
        m_selfNodeIdKnown = true;
#endif
    }
}

void VideoOutputNode::inputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.inPortIndex != 0)
        return;

    // The port-0 "video-frame" edge was removed. Disconnecting does NOT stop
    // the source player — frames keep flowing into setInData(), so first clear
    // the connection flag that guards the port-0 branch, then close the
    // detached popup and reset all video state (mirror of the destructor).
    m_videoInputConnected = false;

    if (m_glWidget != nullptr) {
        m_glWidget->hide();
        m_glWidget->deleteLater();
        m_glWidget = nullptr;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_videoWidget != nullptr) {
        m_videoWidget->hide();
        m_videoWidget->deleteLater();
        m_videoWidget = nullptr;
    }
    // In-scene QGraphicsVideoItem (REQ-SW-PL-021): delete on disconnect so no
    // stale video surface keeps rendering inside the node.
    if (m_sceneVideoItem != nullptr) {
        m_sceneVideoItem->deleteLater();
        m_sceneVideoItem = nullptr;
    }
    // The badge is a top-level window (not a child of m_videoWidget), so close
    // it explicitly to avoid a dangling overlay window.
    if (m_perfBadge != nullptr) {
        m_perfBadge->hide();
        m_perfBadge->deleteLater();
        m_perfBadge = nullptr;
    }
#endif
    m_lastInput.reset();
    m_image = QImage();
    m_output.reset();
    m_label->setText(tr("No video input"));
    updateDisplay();
}

QWidget *VideoOutputNode::embeddedWidget()
{
    return m_widget;
}

QVideoFrame VideoOutputNode::presentableFrame(
    const std::shared_ptr<VideoFrameData> &frame) const
{
    if (frame->isGpuRgba()) {
        // GPU-resident RGBA frame (effect output): the native sinks cannot
        // consume a raw GL texture — readback at the display boundary
        // (REQ-SW-PL-032 AC 5; Stage 2C will present the texture directly).
        return QVideoFrame(frame->asImage());
    }
    return frame->frame();
}

bool VideoOutputNode::eventFilter(QObject *object, QEvent *event)
{
    // In GL blit mode the detached GL window has its own size — label resizes
    // must not trigger per-frame re-presents. The label is the active display
    // surface only while no GL window exists (software path / fallback).
    if (object == m_label && event->type() == QEvent::Resize) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // In-scene mode (REQ-SW-PL-021): keep the QGraphicsVideoItem aligned
        // over the label area when the embedded widget is resized.
        if (m_sceneVideoItem != nullptr)
            updateSceneVideoItemGeometry();
#endif
        if (m_glWidget == nullptr)
            updateDisplay();
    }
    return false;
}

void VideoOutputNode::updateDisplay()
{
    // In-scene mode (Qt6 checkbox OFF / Qt5 software) must never open a
    // detached GL window — m_detachedEnabled guards the GL blit branch. On Qt5
    // m_detachedEnabled == m_glEnabled, so behavior is unchanged there.
    if (m_detachedEnabled && m_glEnabled) {
        // GL blit path: present the QImage (image port / RGB fallback)
        // on the detached GL window instead of QPixmap + smooth-scale.
        if (m_image.isNull()) {
            if (m_glWidget != nullptr)
                m_glWidget->hide();
            m_label->setPixmap(QPixmap());
            return;
        }
        ensureGlWidget();
        if (m_glWidget != nullptr) {
            m_glWidget->presentImage(m_image);
            return;
        }
        // GL unavailable — fallbackToSoftware() already ran and cleared
        // m_glEnabled; fall through to the embedded label path.
    }

    if (m_image.isNull()) {
        m_label->setPixmap(QPixmap());
        return;
    }

    QSize targetSize = m_label->size();
    if (!targetSize.isValid() || targetSize.isEmpty())
        targetSize = m_label->minimumSize();

    const QPixmap pixmap = QPixmap::fromImage(m_image);
    m_label->setPixmap(pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void VideoOutputNode::setGlEnabled(bool enabled)
{
    // The checkbox drives the detached-vs-in-scene choice. On Qt5 the detached
    // backend IS the GL blit widget, so m_glEnabled follows the checkbox
    // (unchecked = software QLabel path). On Qt6 the detached backend is fixed
    // by DAQSTER_GL_BLIT at startup (GL blit widget vs native QVideoWidget) and
    // the checkbox only toggles detached vs in-scene (QGraphicsVideoItem).
    if (m_detachedEnabled == enabled)
        return;
    m_detachedEnabled = enabled;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6: the detached backend (m_glEnabled) stays as initialized — the
    // checkbox does not switch GL blit / QVideoWidget, only detached / in-scene.
#else
    m_glEnabled = enabled;
#endif

    if (!enabled) {
        // Detached -> in-scene/software: close the detached windows right away
        // so no orphan window keeps showing; the next frame/image takes the
        // in-scene (Qt6) or embedded-label (Qt5) path (applied "at the next
        // frame", no crash, no video loss).
        if (m_glWidget != nullptr) {
            m_glWidget->hide();
            m_glWidget->deleteLater();
            m_glWidget = nullptr;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (m_videoWidget != nullptr) {
            m_videoWidget->hide();
            m_videoWidget->deleteLater();
            m_videoWidget = nullptr;
        }
#endif
        return;
    }

    // Software/in-scene -> detached: the window is created lazily on the next
    // frame by ensureVideoWidget(). Nothing to do here — but if a previous GL
    // attempt failed this session, do not resurrect a broken window: fall back
    // again.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // In-scene -> detached: destroy the in-scene QGraphicsVideoItem so no
    // stale surface keeps rendering inside the node once the detached window
    // takes over.
    if (m_sceneVideoItem != nullptr) {
        m_sceneVideoItem->deleteLater();
        m_sceneVideoItem = nullptr;
    }
#endif
    if (m_glEnabled && m_glFailed) {
        fallbackToSoftware(QStringLiteral("GL unavailable (previous attempt failed)"));
        return;
    }
}

void VideoOutputNode::fallbackToSoftware(const QString &reason)
{
    qWarning().noquote()
        << QStringLiteral("GL fallback: %1 — switching to software display").arg(reason);
    m_glFailed = true;

    if (m_glWidget != nullptr) {
        m_glWidget->hide();
        m_glWidget->deleteLater();
        m_glWidget = nullptr;
    }

    // Reflect the real display backend in the UI: unchecking fires the toggled
    // handler, which clears m_glEnabled and closes the GL window (already done
    // above). Re-checking later re-attempts GL and falls back again (the
    // session guard m_glFailed prevents a broken window from being created).
    if (m_glCheck != nullptr && m_glCheck->isChecked())
        m_glCheck->setChecked(false);
    else
        m_glEnabled = false;

    // Placeholder until the next frame delivers a software image.
    m_label->setText(tr("GPU display unavailable — using software display"));
}

void VideoOutputNode::ensureGlWidget()
{
    // GL is known unavailable this session: do not create (or resurrect) a
    // broken window. Callers (ensureVideoWidget / updateDisplay) fall through
    // to the software path.
    if (m_glFailed)
        return;

    if (m_glWidget != nullptr) {
        // Disconnecting an input edge only HIDES the GL window (the
        // updateDisplay() null-image branch) — it is not destroyed. Re-show it
        // here so a re-connect brings the detached display back instead of
        // leaving a hidden window behind (Qt5 image port / Qt6 image port).
        if (!m_glWidget->isVisible()) {
            m_glWidget->show();
            m_label->setText(tr("GPU display active — see detached window"));
        }
        return;
    }

    // Pre-probe: never construct the QOpenGLWidget when the platform cannot
    // create a GL context (VM / remote / software rendering without GL).
    if (!glPlatformAvailable()) {
        fallbackToSoftware(QStringLiteral("no GL context can be created"));
        return;
    }

    m_glWidget = new VideoGLBlitWidget();
    m_glWidget->setWindowTitle(tr("Video Output — %1 (GL blit)").arg(caption()));
    m_glWidget->resize(640, 480);
    m_glWidget->show();
    m_label->setText(tr("GPU display active — see detached window"));

    // Auto-fallback (REQ-SW-PL-021): the GL context is created lazily by
    // QOpenGLWidget on the first show/paint. Defer the validity check by one
    // event-loop turn so the context creation has run; if it failed the node
    // falls back to the software path and video keeps displaying at 25 fps.
    QTimer::singleShot(0, this, [this]() {
        if (m_glWidget != nullptr && !m_glWidget->isValid())
            fallbackToSoftware(QStringLiteral("QOpenGLWidget GL context is not valid"));
    });
}

void VideoOutputNode::logPerfLine()
{
    auto &domain = Daqster::Perf::Domain::get("video");
    if (!domain.enabled())
        return;

    // Sample self-CPU first: the first sample only establishes the baseline and
    // returns 0.0 (the "cpu=0.0%" on the very first line is expected).
    const double cpuPercent = m_cpu.sample();

    // Log only once there are actual frame records (count > 0).
    if (domain.count("output.total") <= 0
        && domain.count("source.frame_interval") <= 0) {
        return;
    }

    // Log at Info level WITHOUT a category (qInfo() instead of qCDebug(lcPerf)):
    // the "daqster.perf" category is disabled by default in LogManager, so a
    // qCDebug(lcPerf) line would be silently filtered and the [PERF] report
    // would never reach the console. qInfo() is unconditional (like the FFmpeg
    // [INF] lines) and guarantees the report is always visible when Perf is on.
    qInfo().noquote()
        << formatPerfLine(domain.avg("source.frame_interval"),
                          domain.avg("output.present"),
                          domain.avg("output.total"),
                          cpuPercent,
                          m_lastHandleType, m_lastPixelFormat);
}

void VideoOutputNode::ensureVideoWidget(bool wantGlBlit)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // In-scene mode (REQ-SW-PL-021, checkbox OFF): never create a detached
    // window — the in-scene QGraphicsVideoItem branch in setInData() handles
    // display (with the software QLabel path as fallback).
    if (!m_detachedEnabled)
        return;

    // Stage 2C (REQ-SW-PL-032): the detached backend is selected PER FRAME.
    // wantGlBlit == true → GL blit widget (GpuRgba effect output with
    // hardware GL, or DAQSTER_GL_BLIT=1); wantGlBlit == false → native
    // QVideoWidget (CPU/NV12). Switching between the two destroys the other
    // widget so only one detached display exists at a time (no leak, no
    // stale window, no crash).
    if (wantGlBlit) {
        if (m_glWidget != nullptr)
            return;
        if (m_videoWidget != nullptr) {
            m_videoWidget->hide();
            m_videoWidget->deleteLater();
            m_videoWidget = nullptr;
        }
    } else {
        if (m_videoWidget != nullptr)
            return;
        if (m_glWidget != nullptr) {
            m_glWidget->hide();
            m_glWidget->deleteLater();
            m_glWidget = nullptr;
        }
    }
#else
    if (m_glWidget != nullptr)
        return;
#endif

    // GL blit display path (default on Qt5; Qt6 for GpuRgba frames or when
    // DAQSTER_GL_BLIT=1): use the GL window instead of the QVideoWidget
    // (Qt6) / software label path (Qt5) so the two display backends can be
    // A/B tested. If the GL context cannot be created, ensureGlWidget()
    // falls back to software and the label path in setInData() keeps the
    // video visible.
    if (wantGlBlit) {
        ensureGlWidget();
        if (m_glWidget != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            createPerfBadge();
#endif
            m_label->setText(tr("GPU display active — see detached window"));
        }
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // QVideoWidget is a native QWindow with its own RHI swapchain and cannot
    // be hosted inside the node editor scene (QTBUG-35299). Detach it into a
    // separate top-level window on the first video-frame input.
    m_videoWidget = new QVideoWidget();
    m_videoWidget->setWindowTitle(tr("Video Output — %1").arg(caption()));
    m_videoWidget->resize(640, 480);
    m_videoWidget->show();

    createPerfBadge();

    // The in-node QLabel shows a static placeholder while the GPU path is
    // active — it is not updated per-frame (avoids redundant QImage conversion).
    m_label->setText(tr("GPU display active — see detached window"));
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void VideoOutputNode::createPerfBadge()
{
    // In-scene mode (REQ-SW-PL-021, checkbox OFF): the badge is a detached-
    // window overlay — never create it for the in-scene QGraphicsVideoItem.
    if (!m_detachedEnabled)
        return;
    if (m_perfBadge != nullptr)
        return;

    // Perf overlay badge (REQ-SW-PL-027): a separate top-level frameless tool
    // window (NOT a child of the detached QVideoWidget). QVideoWidget renders
    // the video in its own native layer (RHI swapchain) and does NOT composite
    // child widgets on top of it, so a child QLabel would stay invisible over
    // the video. A frameless, transparent-for-mouse, always-on-top tool window
    // that tracks the video window's position is the reliable approach.
    m_perfBadge = new QLabel(nullptr,
        Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_perfBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_perfBadge->setAttribute(Qt::WA_TranslucentBackground);
    m_perfBadge->setAttribute(Qt::WA_ShowWithoutActivating);
    m_perfBadge->setStyleSheet(
        QStringLiteral("background-color: rgba(0,0,0,140); color: #0f0; padding: 2px;"));
    m_perfBadge->adjustSize();
    m_perfBadge->hide();
    positionPerfBadge();
}

void VideoOutputNode::updatePerfBadge()
{
    if (m_perfBadge == nullptr)
        return;

    // In-scene mode: the badge only tracks detached windows — hide it so a
    // stale overlay does not float over the scene.
    if (!m_detachedEnabled) {
        m_perfBadge->hide();
        return;
    }

    auto &domain = Daqster::Perf::Domain::get("video");
    if (!domain.enabled()) {
        m_perfBadge->hide();
        return;
    }

    m_perfBadge->setText(formatPerfBadge(
        domain.avg("source.frame_interval"),
        domain.avg("output.present"),
        domain.avg("output.total"),
        m_lastHandleType, m_lastPixelFormat));
    m_perfBadge->adjustSize();
    positionPerfBadge();
    m_perfBadge->raise();
    m_perfBadge->show();
}

void VideoOutputNode::positionPerfBadge()
{
    // The badge only tracks detached display windows (REQ-SW-PL-021): no-op in
    // in-scene mode.
    if (!m_detachedEnabled)
        return;
    if (m_perfBadge == nullptr)
        return;

    QWidget *displayWindow = (m_glWidget != nullptr)
        ? static_cast<QWidget *>(m_glWidget)
        : static_cast<QWidget *>(m_videoWidget);
    if (displayWindow == nullptr)
        return;

    // The badge is a top-level window, so it must be positioned in global
    // coordinates. Pin it to the top-left corner of the video window (client
    // area origin mapped to global), offset by a few pixels.
    const QPoint topLeft = displayWindow->mapToGlobal(QPoint(0, 0)) + QPoint(4, 4);
    m_perfBadge->move(topLeft);
}

void VideoOutputNode::ensureSceneVideoItem()
{
    if (m_sceneVideoItem != nullptr || !m_selfNodeIdKnown)
        return;

    // NodeDelegateModel has no direct scene access (REQ-SW-PL-021 design note):
    // locate the node editor GraphicsView among the running top-level widgets,
    // then the DataFlowGraphicsScene that owns the NodeGraphicsObject for this
    // node.
    QtNodes::GraphicsView *view = nullptr;
    const QWidgetList topLevels = QApplication::topLevelWidgets();
    for (QWidget *w : topLevels) {
        view = w->findChild<QtNodes::GraphicsView *>();
        if (view != nullptr)
            break;
    }
    if (view == nullptr)
        return;

    auto *scene = dynamic_cast<QtNodes::DataFlowGraphicsScene *>(view->scene());
    if (scene == nullptr)
        return;

    QtNodes::NodeGraphicsObject *ngo = scene->nodeGraphicsObject(m_selfNodeId);
    if (ngo == nullptr)
        return;

    // The video item renders inside the node and inherits the node's transform
    // (moves/resizes with it). Z-value above the embedded proxy widget so it
    // covers the label area.
    m_sceneVideoItem = new QGraphicsVideoItem(ngo);
    m_sceneVideoItem->setParentItem(ngo);
    m_sceneVideoItem->setZValue(1.0);
    m_sceneVideoItem->setFlag(QGraphicsItem::ItemIgnoresParentOpacity);
    updateSceneVideoItemGeometry();
    m_sceneVideoItem->show();

    m_label->setText(tr("GPU display active — in scene"));

    // One-shot diagnostic (REQ-SW-PL-021): confirms the in-scene item exists
    // and where it was placed over the label area.
    qInfo().noquote() << QStringLiteral(
        "VideoOutputNode: in-scene QGraphicsVideoItem created (nodeId=%1, "
        "pos=(%2,%3), size=%4x%5)")
            .arg(m_selfNodeId)
            .arg(m_sceneVideoItem->pos().x())
            .arg(m_sceneVideoItem->pos().y())
            .arg(m_sceneVideoItem->size().width())
            .arg(m_sceneVideoItem->size().height());
}

void VideoOutputNode::updateSceneVideoItemGeometry()
{
    if (m_sceneVideoItem == nullptr || !m_selfNodeIdKnown)
        return;

    // Same geometry source as the embedded widget (updateQWidgetEmbedPos →
    // widgetPosition) plus the label's own offset inside the widget layout.
    // The proxy widget embeds the QWidget 1:1 in scene units, so the label's
    // top-left in scene coordinates is widgetPosition + label pos-in-widget.
    QtNodes::GraphicsView *view = nullptr;
    const QWidgetList topLevels = QApplication::topLevelWidgets();
    for (QWidget *w : topLevels) {
        view = w->findChild<QtNodes::GraphicsView *>();
        if (view != nullptr)
            break;
    }
    if (view == nullptr)
        return;

    auto *scene = dynamic_cast<QtNodes::DataFlowGraphicsScene *>(view->scene());
    if (scene == nullptr)
        return;

    const QPointF widgetPos = scene->nodeGeometry().widgetPosition(m_selfNodeId);
    const QPointF labelPos = m_label->pos();
    m_sceneVideoItem->setPos(widgetPos + labelPos);
    const QSize labelSize = m_label->size();
    if (labelSize.isValid() && !labelSize.isEmpty())
        m_sceneVideoItem->setSize(QSizeF(labelSize));
}
#endif
