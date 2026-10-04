#include "VideoOutputUi.h"

#include "VideoDisplayBackend.h"
#include "VideoDisplayWidget.h"
#include "VideoGLBlitWidget.h"
#include "VideoSoftwareWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

// QVideoFrame::HandleType → display name. Qt 6 values, valid for both Qt5/Qt6
// because Qt5 frames normalized to Qt6 numbering before arriving here.
QString handleTypeName(int handleType)
{
    switch (handleType) {
    case 0:  return QStringLiteral("NoHandle");
    case 1:  return QStringLiteral("RhiTextureHandle");
    default: return QString::number(handleType);
    }
}

// QVideoFrameFormat::PixelFormat → display name (Qt 6 values). Ported verbatim
// from VideoPerfBadge so the perf panel and the badge never disagree about how a
// format is spelled.
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

VideoOutputUi::VideoOutputUi(const QVector<EffectSpec> &specs, EffectParams *params,
                             IVideoOutputUiHandler *handler, QWidget *parent)
    : m_specs(specs)
    , m_params(params)
    , m_handler(handler)
{
    m_root = new QWidget(parent);
    m_root->installEventFilter(this);

    m_previewPage = createPreviewPage();
    m_livePage = new QWidget();

    m_videoStack = new QStackedWidget(m_root);
    m_videoStack->addWidget(m_previewPage);   // index 0: preview (editor canvas)
    m_videoStack->addWidget(m_livePage);      // index 1: live (deembed / run)

    auto *mainLayout = new QVBoxLayout(m_root);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_videoStack);

    // ── Live page: splitter = display (left) + controls (right) ─────────────
    m_splitter = new QSplitter(Qt::Horizontal, m_livePage);

    // Unified display (REQ-SW-PL-055): ONE display widget, backend chosen ONCE at
    // construction. Hardware GL → GL blit (zero-copy), otherwise software.
    // DAQSTER_VIDEO_BACKEND=gl|software overrides the auto-detect.
    m_display = (detectVideoBackend() == VideoBackend::Gl)
        ? static_cast<VideoDisplayWidget *>(new VideoGLBlitWidget(m_livePage))
        : static_cast<VideoDisplayWidget *>(new VideoSoftwareWidget(m_livePage));
    m_display->widget()->setMinimumSize(320, 240);
    m_splitter->addWidget(m_display->widget());      // Left: video (stretch = 1)
    m_splitter->setStretchFactor(0, 1);

    m_controlsWidget = createControlsWidget();
    m_controlsWidget->setMinimumWidth(220);
    m_splitter->addWidget(m_controlsWidget);          // Right: controls
    m_splitter->setCollapsible(1, true);
    m_splitter->setHandleWidth(8);
    m_splitter->setChildrenCollapsible(true);
    m_splitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #555; border: 1px solid #333; }"
        "QSplitter::handle:hover { background: #888; }"
        "QSplitter::handle:horizontal { border-left: 2px solid #333;"
        "                             border-right: 2px solid #333; }"));
    m_splitter->setSizes(QList<int>({320, 220}));

    auto *liveLayout = new QVBoxLayout(m_livePage);
    liveLayout->setContentsMargins(0, 0, 0, 0);
    liveLayout->addWidget(m_splitter);

    // No proxy yet → live page. Corrected on the first Show via the event filter
    // and by whichever side calls setLiveActive() first.
    m_videoStack->setCurrentWidget(m_livePage);
}

VideoOutputUi::~VideoOutputUi() = default;

QWidget *VideoOutputUi::createPreviewPage()
{
    m_preview = new QLabel();
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(320, 240);
    m_preview->setText(tr("No video"));
    m_preview->setStyleSheet(QStringLiteral(
        "QLabel { background: #1e1e1e; color: #888; border: 1px solid #444; }"));

    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_preview);
    return page;
}

QWidget *VideoOutputUi::createControlsWidget()
{
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_perfToggle = new QCheckBox(tr("Perf"), widget);
    m_perfToggle->setToolTip(
        tr("Enable/disable performance profiling for video pipeline"));
    m_perfToggle->setChecked(false);   // Default off — the user opts in.
    layout->addWidget(m_perfToggle);

    auto *perfPanel = new QWidget();
    auto *perfLayout = new QFormLayout(perfPanel);
    m_fpsLabel = new QLabel(QStringLiteral("--"));
    m_gapLabel = new QLabel(QStringLiteral("--"));
    m_presentLabel = new QLabel(QStringLiteral("--"));
    m_totalLabel = new QLabel(QStringLiteral("--"));
    m_cpuLabel = new QLabel(QStringLiteral("--"));
    m_hwSwLabel = new QLabel(QStringLiteral("--"));
    m_formatLabel = new QLabel(QStringLiteral("--"));
    m_handleLabel = new QLabel(QStringLiteral("--"));
    perfLayout->addRow(QStringLiteral("FPS:"), m_fpsLabel);
    perfLayout->addRow(QStringLiteral("Gap (ms):"), m_gapLabel);
    perfLayout->addRow(QStringLiteral("Present (ms):"), m_presentLabel);
    perfLayout->addRow(QStringLiteral("Total (ms):"), m_totalLabel);
    perfLayout->addRow(QStringLiteral("CPU (%):"), m_cpuLabel);
    perfLayout->addRow(QStringLiteral("HW/SW:"), m_hwSwLabel);
    perfLayout->addRow(QStringLiteral("Format:"), m_formatLabel);
    perfLayout->addRow(QStringLiteral("Handle:"), m_handleLabel);
    layout->addWidget(perfPanel);

    connect(m_perfToggle, &QCheckBox::toggled, this,
            [this](bool checked) {
                if (m_handler != nullptr)
                    m_handler->onPerfToggled(checked);
                // Unchecking blanks the panel immediately — the refresh timer
                // stops with it, so nothing else would clear these.
                if (!checked)
                    refreshPerfLabels(VideoPerfStats());
            });

    // ── Effect controls (REQ-SW-PL-034), default "No effect" ────────────────
    // Combo index 0 is the placeholder; index i+1 is m_specs[i]. The backend
    // suffix mirrors VideoEffectNode so both nodes list effects identically.
    m_effectCombo = new QComboBox(widget);
    m_effectCombo->setMinimumWidth(190);
    m_effectCombo->addItem(tr("No effect"));
    for (const EffectSpec &spec : m_specs) {
        const QString backendLabel = (spec.backend == EffectSpec::Backend::CpuOnly)
            ? QStringLiteral(" (CPU)")
            : QStringLiteral(" (GPU)");
        m_effectCombo->addItem(spec.displayName + backendLabel);
    }

    m_effectStack = new QStackedWidget(widget);
    m_effectStack->addWidget(new QWidget(m_effectStack));   // blank "No effect"
    for (int i = 0; i < m_specs.size(); ++i)
        m_effectStack->addWidget(createEffectPage(i));

    connect(m_effectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (m_effectStack != nullptr && m_effectStack->currentIndex() != index)
                    m_effectStack->setCurrentIndex(index);
                if (m_handler != nullptr)
                    m_handler->onEffectSelectionChanged(index);
            });

    layout->addWidget(m_effectCombo);
    layout->addWidget(m_effectStack, 1);

    return widget;
}

QWidget *VideoOutputUi::createEffectPage(int specIndex)
{
    const EffectSpec &spec = m_specs[specIndex];
    if (spec.id == QStringLiteral("brightness")) {
        return createSliderPage(m_params->brightness, -100, 100,
                                tr("Brightness (-100..+100)"));
    }
    if (spec.id == QStringLiteral("contrast")) {
        return createSliderPage(m_params->contrast, 0, 200,
                                tr("Contrast (0..200%, 100% = unchanged)"));
    }
    if (spec.id == QStringLiteral("flip"))
        return createFlipPage();
    if (spec.id == QStringLiteral("blur")) {
        return createSliderPage(m_params->blurRadius, 0, 10,
                                tr("Blur radius (0..10)"));
    }
    if (spec.id == QStringLiteral("gaussianBlur")) {
        return createSliderPage(m_params->gaussianKernel, 1, 31,
                                tr("Gaussian kernel (odd, 1..31)"));
    }
    if (spec.id == QStringLiteral("canny"))
        return createCannyPage();
    if (spec.id == QStringLiteral("threshold")) {
        return createSliderPage(m_params->thresholdValue, 0, 255,
                                tr("Threshold value (0..255)"));
    }
    // Parameter-less effects (grayscale/invert/sepia/channelSwap).
    return createInfoPage(spec.displayName);
}

QWidget *VideoOutputUi::createSliderPage(int &value, int min, int max,
                                        const QString &title)
{
    auto *page = new QWidget();
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

    // Writes straight into the model's EffectParams, then asks for a reprocess.
    // No copy, no setter round-trip: the widget IS the editing surface for that
    // field, and _core re-reads the struct.
    connect(slider, &QSlider::valueChanged, this,
            [this, &value, valueLabel](int v) {
                value = v;
                valueLabel->setText(QString::number(v));
                if (m_handler != nullptr)
                    m_handler->onEffectParamsChanged();
            });

    return page;
}

QWidget *VideoOutputUi::createFlipPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *titleLabel = new QLabel(tr("Flip direction"), page);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto *flipCombo = new QComboBox(page);
    flipCombo->addItem(tr("Horizontal"), true);
    flipCombo->addItem(tr("Vertical"), false);
    flipCombo->setCurrentIndex(m_params->flipHorizontal ? 0 : 1);
    layout->addWidget(flipCombo);

    connect(flipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, flipCombo](int) {
                m_params->flipHorizontal = flipCombo->currentData().toBool();
                if (m_handler != nullptr)
                    m_handler->onEffectParamsChanged();
            });

    return page;
}

QWidget *VideoOutputUi::createCannyPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *titleLabel = new QLabel(tr("Canny edge detection thresholds"), page);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto addRow = [this, page, layout](const QString &caption, int &value) {
        auto *row = new QHBoxLayout();
        auto *slider = new QSlider(Qt::Horizontal, page);
        slider->setRange(0, 255);
        slider->setValue(value);
        auto *valueLabel = new QLabel(QString::number(value), page);
        valueLabel->setMinimumWidth(32);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(new QLabel(caption, page));
        row->addWidget(slider, 1);
        row->addWidget(valueLabel);
        layout->addLayout(row);
        connect(slider, &QSlider::valueChanged, this,
                [this, &value, valueLabel](int v) {
                    value = v;
                    valueLabel->setText(QString::number(v));
                    if (m_handler != nullptr)
                        m_handler->onEffectParamsChanged();
                });
    };

    addRow(tr("Low"), m_params->cannyLow);
    addRow(tr("High"), m_params->cannyHigh);

    return page;
}

QWidget *VideoOutputUi::createInfoPage(const QString &text)
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);
    auto *label = new QLabel(text, page);
    label->setWordWrap(true);
    layout->addWidget(label);
    return page;
}

void VideoOutputUi::setLiveActive(bool live)
{
    if (m_videoStack == nullptr)
        return;
    m_videoStack->setCurrentWidget(live ? m_livePage : m_previewPage);
}

bool VideoOutputUi::isLiveActive() const
{
    return m_videoStack != nullptr && m_videoStack->currentWidget() == m_livePage;
}

bool VideoOutputUi::isEmbeddedInProxy() const
{
    return m_root != nullptr && m_root->graphicsProxyWidget() != nullptr;
}

void VideoOutputUi::setPreviewImage(const QImage &image)
{
    if (m_preview == nullptr)
        return;
    if (image.isNull()) {
        setPlaceholder(tr("No video"));
        return;
    }
    m_preview->setPixmap(QPixmap::fromImage(image));
}

void VideoOutputUi::clearPreview()
{
    setPlaceholder(tr("No video"));
}

void VideoOutputUi::setPlaceholder(const QString &text)
{
    if (m_preview == nullptr)
        return;
    m_preview->setText(text);
    m_preview->setPixmap(QPixmap());
}

bool VideoOutputUi::previewVisible() const
{
    return m_preview != nullptr && m_preview->isVisible();
}

void VideoOutputUi::setEffectPage(int comboIndex)
{
    if (m_effectCombo != nullptr && m_effectCombo->currentIndex() != comboIndex) {
        const QSignalBlocker blocker(m_effectCombo);
        m_effectCombo->setCurrentIndex(comboIndex);
    }
    if (m_effectStack != nullptr && m_effectStack->currentIndex() != comboIndex)
        m_effectStack->setCurrentIndex(comboIndex);
}

void VideoOutputUi::syncEffectParams()
{
    // Rebuilds the pages so every widget shows the value now sitting in
    // m_params, which is the EffectParams instance owned by the model — load()
    // has already written into it by the time this is called.
    //
    // Rebuilding rather than walking the widget tree for sliders by index: the
    // restored graph can arrive before the pages exist, and EffectParams has no
    // identity a widget could be matched against. The pages hold no state of
    // their own — every field writes straight into m_params — so rebuilding
    // loses nothing.
    if (m_effectStack == nullptr)
        return;
    const int index = m_effectStack->currentIndex();
    while (m_effectStack->count() > 0) {
        QWidget *page = m_effectStack->widget(0);
        m_effectStack->removeWidget(page);
        page->deleteLater();
    }
    m_effectStack->addWidget(new QWidget(m_effectStack));
    for (int i = 0; i < m_specs.size(); ++i)
        m_effectStack->addWidget(createEffectPage(i));
    if (index >= 0)
        m_effectStack->setCurrentIndex(index);
}

void VideoOutputUi::updatePerfStats(const VideoPerfStats &stats)
{
    refreshPerfLabels(stats);
}

void VideoOutputUi::refreshPerfLabels(const VideoPerfStats &stats)
{
    const QString dash = QStringLiteral("--");
    m_fpsLabel->setText(stats.fps > 0.0 ? QString::number(stats.fps, 'f', 1) : dash);
    m_gapLabel->setText(stats.gapMs > 0.0 ? QString::number(stats.gapMs, 'f', 1) : dash);
    m_presentLabel->setText(
        stats.presentMs > 0.0 ? QString::number(stats.presentMs, 'f', 1) : dash);
    m_totalLabel->setText(
        stats.totalMs > 0.0 ? QString::number(stats.totalMs, 'f', 1) : dash);
    m_cpuLabel->setText(
        stats.cpuPercent > 0.0 ? QString::number(stats.cpuPercent, 'f', 1) : dash);
    // HW/SW comes from the backend itself, so the panel can never disagree with
    // what is actually presenting.
    m_hwSwLabel->setText(stats.hardwareGL ? QStringLiteral("HW") : QStringLiteral("SW"));
    m_formatLabel->setText(pixelFormatName(stats.pixelFormat));
    m_handleLabel->setText(handleTypeName(stats.handleType));
}

void VideoOutputUi::setPerfChecked(bool checked)
{
    if (m_perfToggle != nullptr && m_perfToggle->isChecked() != checked)
        m_perfToggle->setChecked(checked);
}

bool VideoOutputUi::perfChecked() const
{
    return m_perfToggle != nullptr && m_perfToggle->isChecked();
}

QWidget *VideoOutputUi::displayWidget() const
{
    return m_display != nullptr ? m_display->widget() : nullptr;
}

bool VideoOutputUi::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_root
        && (event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        if (m_handler != nullptr)
            m_handler->onUiVisibilityChanged(isEmbeddedInProxy());
    }
    return QObject::eventFilter(watched, event);
}
