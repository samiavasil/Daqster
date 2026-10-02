#include "VideoEffectWidget.h"

#include "VideoEffectOps.h"
#include "VideoEffectNode.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

VideoEffectWidget::VideoEffectWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    m_effectCombo = new QComboBox(this);
    m_effectCombo->setMinimumWidth(190);
    layout->addWidget(m_effectCombo);

    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack, 1);

    m_metricLabel = new QLabel(QStringLiteral("CPU --"), this);
    m_metricLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_metricLabel);

    connect(m_effectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VideoEffectWidget::effectIndexChanged);

    // Build pages on first show (lazy init so the model is ready)
    createPages();
}

void VideoEffectWidget::createPages()
{
    // Parameter specifications matching the core model's buildWidget() order:
    // brightness, contrast, grayscale, invert, sepia, channelSwap, flip,
    // blur, gaussianBlur, canny, threshold.
    m_paramSpecs = {
        { -100, 100, 1, QT_TR_NOOP("Brightness (-100..+100)"), "brightness" },
        { 0, 200, 1, QT_TR_NOOP("Contrast (0..200%, 100% = unchanged)"), "contrast" },
        { 0, 0, 0, QT_TR_NOOP("Converts every frame to grayscale."), "" }, // grayscale - info
        { 0, 0, 0, QT_TR_NOOP("Inverts the colors of every frame."), "" }, // invert - info
        { 0, 0, 0, QT_TR_NOOP("Applies a sepia tone to every frame."), "" }, // sepia - info
        { 0, 0, 0, QT_TR_NOOP("Swaps the red and blue channels (R<->B) on every frame."), "" }, // channelSwap - info
        { 0, 1, 1, QT_TR_NOOP("Flip direction"), "flip" }, // flip - combo
        { 0, 10, 1, QT_TR_NOOP("Blur radius (0..10)"), "blurRadius" }, // blur
#ifdef HAVE_OPENCV
        { 1, 31, 2, QT_TR_NOOP("Kernel size (odd, 1..31)"), "gaussianKernel" }, // gaussian
        { 0, 255, 1, QT_TR_NOOP("Low threshold"), "cannyLow" }, // canny low
        { 0, 255, 1, QT_TR_NOOP("High threshold"), "cannyHigh" }, // canny high
        { 0, 255, 1, QT_TR_NOOP("Threshold value (0..255)"), "thresholdValue" }, // threshold
#endif
    };

    // Clear existing pages
    while (!m_pages.isEmpty()) {
        delete m_pages.last().page;
        m_pages.removeLast();
    }

    // Create one page per effect spec
    m_pages.reserve(m_paramSpecs.size());
    for (int i = 0; i < m_paramSpecs.size(); ++i) {
        const ParamSpec& ps = m_paramSpecs[i];
        PageWidgets pw;

        if (ps.paramName[0] == '\0' && i != 6) { // info-only pages (not flip)
            // Info-only effect
            pw.page = new QWidget(m_stack);
            auto* pageLayout = new QVBoxLayout(pw.page);
            pageLayout->setContentsMargins(4, 4, 4, 4);

            auto* label = new QLabel(tr(ps.title), pw.page);
            label->setWordWrap(true);
            pageLayout->addWidget(label);

        } else if (i == 6) { // flip - combo
            pw.page = new QWidget(m_stack);
            auto* pageLayout = new QVBoxLayout(pw.page);
            pageLayout->setContentsMargins(4, 4, 4, 4);

            auto* titleLabel = new QLabel(tr(ps.title), pw.page);
            titleLabel->setWordWrap(true);
            pageLayout->addWidget(titleLabel);

            pw.combo = new QComboBox(pw.page);
            pw.combo->addItem(tr("Horizontal"), true);
            pw.combo->addItem(tr("Vertical"), false);
            pageLayout->addWidget(pw.combo);

            connect(pw.combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this](int index) {
                        Q_EMIT flipChanged(index);
                    });

        } else {
            // Slider-based effects
            pw.page = new QWidget(m_stack);
            auto* pageLayout = new QVBoxLayout(pw.page);
            pageLayout->setContentsMargins(4, 4, 4, 4);

            auto* titleLabel = new QLabel(tr(ps.title), pw.page);
            titleLabel->setWordWrap(true);
            pageLayout->addWidget(titleLabel);

            auto* row = new QHBoxLayout();
            pw.slider = new QSlider(Qt::Horizontal, pw.page);
            pw.slider->setRange(ps.min, ps.max);
            pw.slider->setSingleStep(ps.step);
            pw.valueLabel = new QLabel(QStringLiteral("0"), pw.page);
            pw.valueLabel->setMinimumWidth(32);
            pw.valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            row->addWidget(pw.slider, 1);
            row->addWidget(pw.valueLabel);
            pageLayout->addLayout(row);

            const QString paramName = ps.paramName;
            connect(pw.slider, &QSlider::valueChanged, this,
                [this, paramName, pw](int value) {
                    if (pw.valueLabel) pw.valueLabel->setText(QString::number(value));
                    if (paramName == "brightness") Q_EMIT brightnessChanged(value);
                    else if (paramName == "contrast") Q_EMIT contrastChanged(value);
                    else if (paramName == "blurRadius") Q_EMIT blurChanged(value);
#ifdef HAVE_OPENCV
                    else if (paramName == "gaussianKernel") Q_EMIT gaussianChanged(value);
                    else if (paramName == "cannyLow") Q_EMIT cannyLowChanged(value);
                    else if (paramName == "cannyHigh") Q_EMIT cannyHighChanged(value);
                    else if (paramName == "thresholdValue") Q_EMIT thresholdChanged(value);
#endif
                });
        }

        m_stack->addWidget(pw.page);
        m_pages.append(pw);
    }

    // Populate combo with display names from VideoEffectOps
    const QVector<EffectSpec> specs = VideoEffectOps::allSpecs();
    const QSignalBlocker block(m_effectCombo);
    m_effectCombo->clear();
    for (const EffectSpec& spec : specs) {
        const QString backendLabel = (spec.backend == EffectSpec::Backend::CpuOnly)
            ? QStringLiteral(" (CPU)")
            : QStringLiteral(" (GPU)");
        m_effectCombo->addItem(spec.displayName + backendLabel);
    }
}

void VideoEffectWidget::setConfig(int effectIndex, const EffectParams& params)
{
    // Switch combo and stack
    const QSignalBlocker blockCombo(m_effectCombo);
    if (effectIndex >= 0 && effectIndex < m_effectCombo->count()) {
        m_effectCombo->setCurrentIndex(effectIndex);
    }
    if (effectIndex >= 0 && effectIndex < m_stack->count()) {
        m_stack->setCurrentIndex(effectIndex);
    }

    // Update sliders/labels from params
    if (!m_pages.isEmpty() && effectIndex >= 0 && effectIndex < m_pages.size()) {
        const PageWidgets& pw = m_pages[effectIndex];

        if (pw.slider && pw.valueLabel) {
            int value = 0;
            if (effectIndex == 0) value = params.brightness;           // brightness
            else if (effectIndex == 1) value = params.contrast;        // contrast
            else if (effectIndex == 7) value = params.blurRadius;      // blur
#ifdef HAVE_OPENCV
            else if (effectIndex == 8) value = params.gaussianKernel;  // gaussian
            else if (effectIndex == 9) value = params.cannyLow;        // canny low
            else if (effectIndex == 10) value = params.cannyHigh;      // canny high
            else if (effectIndex == 11) value = params.thresholdValue; // threshold
#endif
            const QSignalBlocker blockSlider(pw.slider);
            pw.slider->setValue(value);
            pw.valueLabel->setText(QString::number(value));
        }

        if (pw.combo) { // flip
            const QSignalBlocker blockCombo(pw.combo);
            pw.combo->setCurrentIndex(params.flipHorizontal ? 0 : 1);
        }
    }

    updateMetricLabel();
}

void VideoEffectWidget::onGpuPathUsed()
{
    m_metricLabel->setText(tr("GPU (GUI thread)"));
}

void VideoEffectWidget::onCpuPathUsed()
{
    updateMetricLabel();
}

void VideoEffectWidget::updateMetricLabel()
{
    // Placeholder - in a full implementation the widget would query
    // the model or pool for counters. For now just show the label format.
    m_metricLabel->setText(QStringLiteral("CPU %1/%2 · %3 skipped · %4 fps out")
                               .arg(m_completed)
                               .arg(m_submitted)
                               .arg(m_skipped)
                               .arg(m_fps, 0, 'f', 1));
}