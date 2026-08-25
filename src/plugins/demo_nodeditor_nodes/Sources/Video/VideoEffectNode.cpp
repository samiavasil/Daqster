#include "VideoEffectNode.h"

#include "NodeDataTypes/VideoFrameData.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoEffectNode::VideoEffectNode(const EffectSpec &spec)
    : m_spec(spec)
{
    buildWidget();
}

VideoEffectNode::~VideoEffectNode()
{
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QString VideoEffectNode::caption() const
{
    return m_spec.displayName;
}

QJsonObject VideoEffectNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    obj[QStringLiteral("effect")] = m_spec.id;
    obj[QStringLiteral("brightness")] = m_params.brightness;
    obj[QStringLiteral("contrast")] = m_params.contrast;
    obj[QStringLiteral("flipMode")] = m_params.flipHorizontal
        ? QStringLiteral("horizontal")
        : QStringLiteral("vertical");
    return obj;
}

void VideoEffectNode::load(QJsonObject const &p)
{
    m_params.brightness = p.value(QStringLiteral("brightness")).toInt(m_params.brightness);
    m_params.contrast = p.value(QStringLiteral("contrast")).toInt(m_params.contrast);
    m_params.flipHorizontal = (p.value(QStringLiteral("flipMode")).toString()
                               != QStringLiteral("vertical"));

    m_params.brightness = std::max(-100, std::min(100, m_params.brightness));
    m_params.contrast = std::max(0, std::min(200, m_params.contrast));

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

    const QVideoFrame &frame = m_lastInput->frame();

    // Runtime backend selection (REQ-SW-PL-028 AC 2/3): CPU-only effects always
    // run on the CPU; GpuOrCpu effects run on the GPU only with hardware GL.
    const bool useGpu = (m_spec.backend == EffectSpec::Backend::GpuOrCpu)
        && VideoEffectGLProcessor::hasHardwareGL();

    if (useGpu) {
        const QImage result = m_glProcessor.process(frame, m_spec, m_params);
        if (!result.isNull()) {
            m_output = std::make_shared<VideoFrameData>(QVideoFrame(result));
            Q_EMIT dataUpdated(0);
            return;
        }
        // GPU path failed (unsupported frame format / GL error) — CPU fallback.
    }

    // CPU path: reuse the lazy QImage cache on the shared VideoFrameData
    // (REQ-SW-PL-032) — the conversion happens at most once per frame and is
    // shared between all CPU consumers. m_lastInput is non-null here (checked
    // above), so asImage() is safe.
    const QImage img = m_lastInput->asImage();
    const QImage transformed = applyCpu(img);
    if (transformed.isNull()) {
        Q_EMIT dataInvalidated(0);
        return;
    }

    m_output = std::make_shared<VideoFrameData>(QVideoFrame(transformed));
    Q_EMIT dataUpdated(0);
}

QWidget *VideoEffectNode::embeddedWidget()
{
    return m_widget;
}

void VideoEffectNode::buildWidget()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    if (m_spec.id == QStringLiteral("brightness")) {
        layout->addWidget(createSliderPage(
            m_brightnessSlider, m_brightnessValue, -100, 100,
            m_params.brightness, tr("Brightness (-100..+100)"),
            [this](int value) {
                m_params.brightness = value;
                reprocessCurrentFrame();
            }));
    } else if (m_spec.id == QStringLiteral("contrast")) {
        layout->addWidget(createSliderPage(
            m_contrastSlider, m_contrastValue, 0, 200,
            m_params.contrast, tr("Contrast (0..200%, 100% = unchanged)"),
            [this](int value) {
                m_params.contrast = value;
                reprocessCurrentFrame();
            }));
    } else if (m_spec.id == QStringLiteral("flip")) {
        layout->addWidget(createFlipPage());
    } else {
        QString info;
        if (m_spec.id == QStringLiteral("grayscale"))
            info = tr("Converts every frame to grayscale.");
        else if (m_spec.id == QStringLiteral("invert"))
            info = tr("Inverts the colors of every frame.");
        else if (m_spec.id == QStringLiteral("sepia"))
            info = tr("Applies a sepia tone to every frame.");
        else if (m_spec.id == QStringLiteral("channelSwap"))
            info = tr("Swaps the red and blue channels (R<->B) on every frame.");
        else
            info = tr("No parameters.");
        auto *label = new QLabel(info, m_widget);
        label->setWordWrap(true);
        layout->addWidget(label);
    }
}

QWidget *VideoEffectNode::createSliderPage(QSlider *&sliderOut, QLabel *&valueLabelOut,
                                           int min, int max, int initial, const QString &title,
                                           std::function<void(int)> onChanged)
{
    auto *page = new QWidget(m_widget);
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
    auto *page = new QWidget(m_widget);
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

void VideoEffectNode::syncWidgetsFromParams()
{
    if (m_brightnessSlider)
        m_brightnessSlider->setValue(m_params.brightness);
    if (m_contrastSlider)
        m_contrastSlider->setValue(m_params.contrast);
    if (m_flipCombo)
        m_flipCombo->setCurrentIndex(m_params.flipHorizontal ? 0 : 1);
}

void VideoEffectNode::reprocessCurrentFrame()
{
    if (!m_lastInput || !m_lastInput->hasFrame())
        return;
    setInData(m_lastInput, 0);
}

QImage VideoEffectNode::applyCpu(const QImage &source) const
{
    if (!m_spec.cpuApply)
        return source;
    return m_spec.cpuApply(source, m_params);
}