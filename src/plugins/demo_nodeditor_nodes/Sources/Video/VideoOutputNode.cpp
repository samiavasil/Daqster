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
#include <QCheckBox>
#include <QDateTime>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

// QVideoFrame::HandleType → display name (Qt 6 values, works for both Qt5/Qt6)
QString handleTypeName(int handleType)
{
    switch (handleType) {
    case 0:  return QStringLiteral("NoHandle");
    case 1:  return QStringLiteral("RhiTextureHandle");
    default: return QString::number(handleType);
    }
}

// QVideoFrameFormat::PixelFormat → display name (Qt 6 values)
QString pixelFormatName(int pixelFormat)
{
    switch (pixelFormat) {
    case -1: return QStringLiteral("Invalid");
    case 0:  return QStringLiteral("Invalid");
    case 1:  return QStringLiteral("ARGB8888");
    case 2:  return QStringLiteral("ARGB8888_Premultiplied");
    case 3:  return QStringLiteral("XRGB8888");
    case 4:  return QStringLiteral("BGRA8888");
    case 5:  return QStringLiteral("BGRA8888_Premultiplied");
    case 6:  return QStringLiteral("BGRX8888");
    case 7:  return QStringLiteral("ABGR8888");
    case 8:  return QStringLiteral("XBGR8888");
    case 9:  return QStringLiteral("RGBA8888");
    case 10: return QStringLiteral("RGBX8888");
    case 11: return QStringLiteral("AYUV");
    case 12: return QStringLiteral("AYUV_Premultiplied");
    case 13: return QStringLiteral("YUV420P");
    case 14: return QStringLiteral("YUV422P");
    case 15: return QStringLiteral("YV12");
    case 16: return QStringLiteral("UYVY");
    case 17: return QStringLiteral("YUYV");
    case 18: return QStringLiteral("NV12");
    case 19: return QStringLiteral("NV21");
    case 20: return QStringLiteral("IMC1");
    case 21: return QStringLiteral("IMC2");
    case 22: return QStringLiteral("IMC3");
    case 23: return QStringLiteral("IMC4");
    case 24: return QStringLiteral("Y8");
    case 25: return QStringLiteral("Y16");
    case 26: return QStringLiteral("P010");
    case 27: return QStringLiteral("P016");
    case 28: return QStringLiteral("SamplerExternalOES");
    case 29: return QStringLiteral("Jpeg");
    case 30: return QStringLiteral("SamplerRect");
    case 31: return QStringLiteral("YUV420P10");
    default: return QString::number(pixelFormat);
    }
}

} // namespace

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

    // ── In-node preview (Option B) ──────────────────────────────────────────
    // A REGULAR QLabel (no QOpenGLWidget) showing a scaled QImage snapshot at
    // ~1–2 fps. No GL in the node scene → no scene repaint problem. The node
    // widget contains ONLY this label — no controls, no splitter, no GL.
    m_preview = new QLabel(m_widget);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(320, 240);
    m_preview->setText(tr("No video"));
    m_preview->setStyleSheet(QStringLiteral(
        "QLabel { background: #1e1e1e; color: #888; border: 1px solid #444; }"));

    QVBoxLayout *mainLayout = new QVBoxLayout(m_widget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_preview);

    // NOTE: the detached display window (m_displayWindow + m_display + splitter
    // + controls) is NOT created here — it is created lazily on first frame
    // arrival (ensureDisplayWindow(), Option B).

    // ── Controls widget (right pane of detached splitter) ───────────────────
    // Built here in the constructor but NOT parented to m_widget — it will be
    // reparented to m_displayWindow when the detached window is created.
    m_controlsWidget = new QWidget();
    QVBoxLayout *controlsLayout = new QVBoxLayout(m_controlsWidget);
    controlsLayout->setContentsMargins(4, 4, 4, 4);
    controlsLayout->setSpacing(4);

    // Perf toggle (visible checkbox at top of controls panel)
    m_perfToggle = new QCheckBox(tr("Perf"), m_controlsWidget);
    m_perfToggle->setToolTip(tr("Enable/disable performance profiling for video pipeline"));
    m_perfToggle->setChecked(false);  // Default off — user opts in
    controlsLayout->addWidget(m_perfToggle);

    // Perf stats panel (simple QFormLayout for now)
    QWidget *perfPanel = new QWidget();
    QFormLayout *perfLayout = new QFormLayout(perfPanel);
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

    // Connect perf toggle: on = enable domain + start timer, off = disable + stop
    connect(m_perfToggle, &QCheckBox::toggled, this, [this](bool checked) {
        auto &domain = Daqster::Perf::Domain::get("video");
        domain.setEnabled(checked);
        if (checked) {
            m_perfRefreshTimer->start();
        } else {
            m_perfRefreshTimer->stop();
            // Clear labels when perf is disabled
            m_fpsLabel->setText("--");
            m_gapLabel->setText("--");
            m_presentLabel->setText("--");
            m_totalLabel->setText("--");
            m_cpuLabel->setText("--");
            m_hwSwLabel->setText("--");
            m_formatLabel->setText("--");
            m_handleLabel->setText("--");
        }
    });

    // Embedded effects (REQ-SW-PL-034): optional, default "No effect" — the
    // zero-copy passthrough is preserved until the user selects an effect.
    buildEffectControls();

    // Add effect controls to the controls widget
    controlsLayout->addWidget(m_effectCombo);
    controlsLayout->addWidget(m_effectStack, 1);

    m_controlsWidget->setMinimumWidth(220);

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

        // All perf domain values are in nanoseconds (recorded by PERF_SCOPE).
        const qint64 frameIntervalNs = domain.avg("source.frame_interval");
        const qint64 presentNs = domain.avg("output.present");
        const qint64 totalNs = domain.avg("output.total");

        const double fps = frameIntervalNs > 0 ? 1e9 / static_cast<double>(frameIntervalNs) : 0.0;
        const double gapMs = frameIntervalNs > 0 ? static_cast<double>(frameIntervalNs) / 1e6 : 0.0;
        const double presentMs = presentNs > 0 ? static_cast<double>(presentNs) / 1e6 : 0.0;
        const double totalMs = totalNs > 0 ? static_cast<double>(totalNs) / 1e6 : 0.0;
        const double cpuPercent = m_cpu.sample();

        m_fpsLabel->setText(QString::number(fps, 'f', 1));
        m_gapLabel->setText(QString::number(gapMs, 'f', 1));
        m_presentLabel->setText(QString::number(presentMs, 'f', 1));
        m_totalLabel->setText(QString::number(totalMs, 'f', 1));
        m_cpuLabel->setText(QString::number(cpuPercent, 'f', 1));

        // HW/SW: use the display backend's GPU status (REQ-SW-PL-053)
        const QString hwSw = (m_display && m_display->isGpuBackend()) ? QStringLiteral("HW") : QStringLiteral("SW");
        m_hwSwLabel->setText(hwSw);

        // Format: readable string from pixel format (Qt6 numbering, matches VideoPerfBadge)
        m_formatLabel->setText(pixelFormatName(m_lastPixelFormat));
        m_handleLabel->setText(handleTypeName(m_lastHandleType));
    });

    // Timer starts only when perf checkbox is checked (default off)

    // ── Preview refresh timer (Option B) ────────────────────────────────────
    // Throttled snapshot: fires every 750 ms (~1.3 fps), converts the latest
    // frame to QImage and updates the in-node QLabel. Started on first frame
    // arrival, stopped on flow stop / input disconnect.
    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(750);
    connect(m_previewTimer, &QTimer::timeout, this, [this]() {
        updatePreview();
    });
}

void VideoOutputNode::updatePreview()
{
    if (m_preview == nullptr)
        return;

    // No frame yet → keep the placeholder text.
    if (!m_lastFrameForPreview || !m_lastFrameForPreview->hasFrame()) {
        m_preview->setText(tr("No video"));
        m_preview->setPixmap(QPixmap());
        return;
    }

    // Convert the latest frame to QImage (cached per frame by VideoFrameData,
    // so repeated timer ticks on the same frame are cheap) and scale it to the
    // label size keeping aspect ratio. SmoothTransformation keeps the snapshot
    // readable at small preview sizes.
    const QImage image = m_lastFrameForPreview->asImage();
    if (image.isNull()) {
        m_preview->setText(tr("No video"));
        m_preview->setPixmap(QPixmap());
        return;
    }

    const QSize labelSize = m_preview->size();
    QSize scaled = image.size();
    if (labelSize.isValid() && !labelSize.isEmpty()) {
        scaled = image.size().scaled(labelSize, Qt::KeepAspectRatio);
    }
    const QPixmap pixmap = QPixmap::fromImage(
        scaled == image.size() ? image : image.scaled(scaled, Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation));
    m_preview->setPixmap(pixmap);
}

void VideoOutputNode::ensureDisplayWindow()
{
    // Create the detached window + splitter + unified display widget on first
    // use (Option B). The window is a top-level QWidget (Qt::Window flag) — it
    // is NOT part of the node scene, so the GL display cannot trigger scene
    // repaints. The splitter houses the video display (left) and controls
    // (right) — the SAME layout that was previously in the node widget.
    if (m_displayWindow == nullptr) {
        m_displayWindow = new QWidget(nullptr, Qt::Window);
        m_displayWindow->setWindowTitle(tr("Video Output — %1").arg(name()));
        m_displayWindow->resize(860, 480);

        // ── Splitter ──────────────────────────────────────────────────────
        m_splitter = new QSplitter(Qt::Horizontal, m_displayWindow);

        // Unified display (REQ-SW-PL-053): ONE display widget, backend
        // selected ONCE at construction. Auto-detect: hardware GL → GL blit
        // (GPU), else software (CPU); DAQSTER_VIDEO_BACKEND=gl|software
        // overrides.
        m_display = (detectVideoBackend() == VideoBackend::Gl)
            ? static_cast<VideoDisplayWidget *>(new VideoGLBlitWidget(m_displayWindow))
            : static_cast<VideoDisplayWidget *>(new VideoSoftwareWidget(m_displayWindow));
        m_display->widget()->setMinimumSize(320, 240);
        m_splitter->addWidget(m_display->widget());      // Left: video (stretch=1)
        m_splitter->setStretchFactor(0, 1);

        // Right pane: controls widget (reparented from constructor)
        m_controlsWidget->setParent(m_displayWindow);
        m_splitter->addWidget(m_controlsWidget);          // Right: controls
        m_splitter->setCollapsible(1, true);
        m_splitter->setHandleWidth(8);
        m_splitter->setChildrenCollapsible(true);
        // Style the splitter handle to be visibly draggable
        m_splitter->setStyleSheet(R"(
            QSplitter::handle {
                background: #555;
                border: 1px solid #333;
            }
            QSplitter::handle:hover {
                background: #888;
            }
            QSplitter::handle:horizontal {
                border-left: 2px solid #333;
                border-right: 2px solid #333;
            }
        )");

        // Restore splitter state if loaded from save()/load().
        if (!m_splitterState.isEmpty()) {
            m_splitter->restoreState(m_splitterState);
        } else {
            // Default sizes: video pane wide, controls at minimum.
            m_splitter->setSizes(QList<int>({640, 220}));
        }

        // Detached window layout: just the splitter, no margins.
        QVBoxLayout *layout = new QVBoxLayout(m_displayWindow);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_splitter);

        // Apply the geometry persisted in save()/load() (if any).
        if (!m_displayWindowGeometry.isEmpty())
            m_displayWindow->restoreGeometry(m_displayWindowGeometry);
    }

    // Show the window on the first frame of a flow. If the user closed the
    // window mid-flow (m_displayWindowShown still true), it is NOT resurrected
    // — it reopens only after a flow stop / input disconnect. A reprocess
    // (effect change / load, m_suppressWindowShow) never shows the window.
    if (!m_suppressWindowShow && !m_displayWindowShown) {
        m_displayWindow->show();
        m_displayWindow->raise();
        m_displayWindowShown = true;
    }
}

void VideoOutputNode::buildEffectControls()
{
    m_specs = VideoEffectOps::allSpecs();

    // Effect combo: index 0 = "No effect" (empty id), then one item per effect
    // with a backend suffix mirroring VideoEffectNode (REQ-SW-PL-028 AC 8).
    m_effectCombo = new QComboBox(m_controlsWidget);
    m_effectCombo->setMinimumWidth(190);
    m_effectCombo->addItem(tr("No effect"));
    for (const EffectSpec &spec : m_specs) {
        const QString backendLabel = (spec.backend == EffectSpec::Backend::CpuOnly)
            ? QStringLiteral(" (CPU)")
            : QStringLiteral(" (GPU)");
        m_effectCombo->addItem(spec.displayName + backendLabel);
    }

    // Parameter stack: page 0 = blank (no effect), page i+1 = effect i.
    m_effectStack = new QStackedWidget(m_controlsWidget);
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

    // The detached display window is a top-level window (no parent) — delete
    // it explicitly. m_display, m_splitter, and m_controlsWidget are its
    // children, so they are destroyed with it.
    delete m_displayWindow;
    m_displayWindow = nullptr;
    m_display = nullptr;
    m_splitter = nullptr;
    m_controlsWidget = nullptr;

    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

void VideoOutputNode::stop()
{
    // Idempotent: stopping an already-stopped timer is a no-op.
    if (m_perfRefreshTimer != nullptr)
        m_perfRefreshTimer->stop();
    if (m_previewTimer != nullptr)
        m_previewTimer->stop();

    // Hide the detached display window (kept for reuse — the widget and its
    // GL context stay alive; the window is re-shown on the next flow's first
    // frame via ensureDisplayWindow()).
    m_displayWindowShown = false;
    if (m_displayWindow != nullptr)
        m_displayWindow->hide();
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

    // Splitter state (REQ-SW-PL-053): persist the QSplitter geometry. The
    // splitter lives in the detached window — it may not exist yet if no frame
    // has arrived (lazy creation). Fall back to the stored m_splitterState.
    if (m_splitter != nullptr) {
        obj[QStringLiteral("splitterState")] =
            QString::fromLatin1(m_splitter->saveState().toBase64());
    } else if (!m_splitterState.isEmpty()) {
        obj[QStringLiteral("splitterState")] =
            QString::fromLatin1(m_splitterState.toBase64());
    }

    // Detached display window (Option B): persist geometry so the window is
    // restored across save/load/restart. The window itself is created lazily
    // on the first frame of a flow (ensureDisplayWindow()).
    if (m_displayWindow != nullptr) {
        obj[QStringLiteral("displayWindowGeometry")] =
            QString::fromLatin1(m_displayWindow->saveGeometry().toBase64());
    }

    // Perf toggle state: persist so the profiling domain + refresh timer are
    // restored across save/load/restart.
    if (m_perfToggle != nullptr)
        obj[QStringLiteral("perfToggle")] = m_perfToggle->isChecked();

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

    // Restore splitter state. The splitter lives in the lazily-created detached
    // window — if it exists already, apply directly; otherwise store in
    // m_splitterState for deferred apply in ensureDisplayWindow().
    const QString splitterStateStr = p.value(QStringLiteral("splitterState")).toString();
    if (!splitterStateStr.isEmpty()) {
        m_splitterState = QByteArray::fromBase64(splitterStateStr.toLatin1());
        if (m_splitter != nullptr)
            m_splitter->restoreState(m_splitterState);
    }

    // Restore detached display window geometry (Option B). The window itself
    // is created lazily on first frame — store the geometry here and apply it
    // in ensureDisplayWindow() when the window is created.
    const QString geometryStr = p.value(QStringLiteral("displayWindowGeometry")).toString();
    if (!geometryStr.isEmpty())
        m_displayWindowGeometry = QByteArray::fromBase64(geometryStr.toLatin1());

    // Restore perf toggle state. Setting checked triggers the toggled signal
    // which enables/disables the perf domain + refresh timer.
    const bool perfChecked = p.value(QStringLiteral("perfToggle")).toBool(false);
    if (m_perfToggle != nullptr)
        m_perfToggle->setChecked(perfChecked);

    // Re-apply the restored effect/parameters to the current frame (mirrors
    // VideoEffectNode::load()). No-op when no frame has arrived yet or the
    // input edge is not connected (the setInData guard returns early).
    reprocessCurrentFrame();
}

void VideoOutputNode::reprocessCurrentFrame()
{
    if (!m_lastInput || !m_lastInput->hasFrame())
        return;
    // A reprocess (effect change / load) must not resurrect the detached
    // window on a stopped flow — only present to an already-created display.
    // The in-node preview still updates (m_lastFrameForPreview is refreshed
    // inside setInData), so the user sees the effect change immediately.
    m_suppressWindowShow = true;
    setInData(m_lastInput, 0);
    m_suppressWindowShow = false;
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

            // ── Detached display (Option B) ────────────────────────────────────────
            // The unified display widget lives in a detached window, created
            // lazily and shown on the first frame of a flow
            // (ensureDisplayWindow). Frames are forwarded to the display
            // widget exactly as before — just in a window now. A reprocess
            // (effect change / load) suppresses the window-show side effect.
            if (m_display != nullptr || (!m_suppressWindowShow && !m_displayWindowShown)) {
                ensureDisplayWindow();
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

            // ── In-node preview (Option B) ──────────────────────────────────
            // Keep the latest frame for the throttled preview timer. The
            // QImage conversion happens in updatePreview() at ~1.3 fps — NOT
            // per frame — so the live path stays zero-copy.
            m_lastFrameForPreview = videoFrame;
            if (m_previewTimer != nullptr && !m_previewTimer->isActive())
                m_previewTimer->start();

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
            m_lastFrameForPreview.reset();
            if (m_previewTimer != nullptr)
                m_previewTimer->stop();
            if (m_preview != nullptr) {
                m_preview->setText(tr("No video"));
                m_preview->setPixmap(QPixmap());
            }
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
    m_lastFrameForPreview.reset();
    if (m_previewTimer != nullptr)
        m_previewTimer->stop();
    if (m_preview != nullptr) {
        m_preview->setText(tr("No video"));
        m_preview->setPixmap(QPixmap());
    }
    if (m_display != nullptr)
        m_display->clear();
    // Hide the detached window (kept for reuse — re-shown on next flow's
    // first frame via ensureDisplayWindow()).
    m_displayWindowShown = false;
    if (m_displayWindow != nullptr)
        m_displayWindow->hide();
}

QWidget *VideoOutputNode::embeddedWidget()
{
    return m_widget;
}