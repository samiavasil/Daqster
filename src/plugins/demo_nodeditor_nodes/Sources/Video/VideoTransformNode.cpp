#include "VideoTransformNode.h"

#include "NodeDataTypes/ImageData.h"
#include "VideoTransformOps.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoTransformNode::VideoTransformNode()
{
    m_operations = buildOperations();
    buildWidget();
}

VideoTransformNode::~VideoTransformNode()
{
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject VideoTransformNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    if (m_operationIndex >= 0 && m_operationIndex < m_operations.size())
        obj[QStringLiteral("operation")] = m_operations[m_operationIndex].id;
    obj[QStringLiteral("brightness")] = m_params.brightness;
    obj[QStringLiteral("contrast")] = m_params.contrast;
    obj[QStringLiteral("blurRadius")] = m_params.blurRadius;
    obj[QStringLiteral("flipMode")] = m_params.flipHorizontal
        ? QStringLiteral("horizontal")
        : QStringLiteral("vertical");
    obj[QStringLiteral("gaussianKernel")] = m_params.gaussianKernel;
    obj[QStringLiteral("cannyLow")] = m_params.cannyLow;
    obj[QStringLiteral("cannyHigh")] = m_params.cannyHigh;
    obj[QStringLiteral("thresholdValue")] = m_params.thresholdValue;
    return obj;
}

void VideoTransformNode::load(QJsonObject const &p)
{
    m_params.brightness = p.value(QStringLiteral("brightness")).toInt(m_params.brightness);
    m_params.contrast = p.value(QStringLiteral("contrast")).toInt(m_params.contrast);
    m_params.blurRadius = p.value(QStringLiteral("blurRadius")).toInt(m_params.blurRadius);
    m_params.flipHorizontal = (p.value(QStringLiteral("flipMode")).toString()
                               != QStringLiteral("vertical"));
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

    // Unknown operation value -> fall back to RGB Channel Swap safely.
    const int index = indexOfOperation(p.value(QStringLiteral("operation")).toString());
    setOperationIndex(index < 0 ? 0 : index);
    syncWidgetsFromParams();
    reprocessCurrentFrame();
}

unsigned int VideoTransformNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoTransformNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return ImageData().type();
}

std::shared_ptr<NodeData> VideoTransformNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void VideoTransformNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    m_lastInput = std::dynamic_pointer_cast<ImageData>(data);
    m_output.reset();

    if (!m_lastInput || m_lastInput->isEmpty()) {
        Q_EMIT dataInvalidated(0);
        return;
    }

    const QImage transformed = applyCurrentOperation(m_lastInput->image());
    if (transformed.isNull()) {
        Q_EMIT dataInvalidated(0);
        return;
    }

    m_output = std::make_shared<ImageData>(transformed);
    Q_EMIT dataUpdated(0);
}

QWidget *VideoTransformNode::embeddedWidget()
{
    return m_widget;
}

QVector<VideoTransformNode::Operation> VideoTransformNode::buildOperations() const
{
    QVector<Operation> ops;
    ops.append({ QStringLiteral("rgbSwap"), tr("RGB Channel Swap"),
                 [](const QImage &img, const TransformParams &) {
                     return VideoTransformOps::swapRedBlue(img);
                 } });
    ops.append({ QStringLiteral("grayscale"), tr("Grayscale"),
                 [](const QImage &img, const TransformParams &) {
                     return VideoTransformOps::grayscale(img);
                 } });
    ops.append({ QStringLiteral("invert"), tr("Invert"),
                 [](const QImage &img, const TransformParams &) {
                     return VideoTransformOps::invert(img);
                 } });
    ops.append({ QStringLiteral("brightness"), tr("Brightness"),
                 [](const QImage &img, const TransformParams &p) {
                     return VideoTransformOps::brightness(img, p.brightness);
                 } });
    ops.append({ QStringLiteral("contrast"), tr("Contrast"),
                 [](const QImage &img, const TransformParams &p) {
                     return VideoTransformOps::contrast(img, p.contrast);
                 } });
    ops.append({ QStringLiteral("blur"), tr("Blur"),
                 [](const QImage &img, const TransformParams &p) {
                     return VideoTransformOps::blur(img, p.blurRadius);
                 } });
    ops.append({ QStringLiteral("flip"), tr("Flip"),
                 [](const QImage &img, const TransformParams &p) {
                     return VideoTransformOps::flip(img, p.flipHorizontal);
                 } });
    ops.append({ QStringLiteral("sepia"), tr("Sepia"),
                 [](const QImage &img, const TransformParams &) {
                     return VideoTransformOps::sepia(img);
                 } });
#ifdef HAVE_OPENCV
    ops.append({ QStringLiteral("gaussianBlur"), tr("Gaussian Blur"),
                 [](const QImage &img, const TransformParams &p) {
                     return VideoTransformOps::gaussianBlur(img, p.gaussianKernel);
                 } });
    ops.append({ QStringLiteral("canny"), tr("Canny Edges"),
                 [](const QImage &img, const TransformParams &p) {
                     return VideoTransformOps::canny(img, p.cannyLow, p.cannyHigh);
                 } });
    ops.append({ QStringLiteral("threshold"), tr("Threshold"),
                 [](const QImage &img, const TransformParams &p) {
                     return VideoTransformOps::threshold(img, p.thresholdValue);
                 } });
#endif
    return ops;
}

int VideoTransformNode::indexOfOperation(const QString &id) const
{
    for (int i = 0; i < m_operations.size(); ++i) {
        if (m_operations[i].id == id)
            return i;
    }
    return -1;
}

void VideoTransformNode::buildWidget()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    m_operationCombo = new QComboBox(m_widget);
    m_operationCombo->setMinimumWidth(190);
    for (const Operation &op : m_operations)
        m_operationCombo->addItem(op.displayName);
    layout->addWidget(m_operationCombo);

    m_stack = new QStackedWidget(m_widget);
    addBaseWidgetPages();
    addOpenCVWidgetPages();
    layout->addWidget(m_stack, 1);

    connect(m_operationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                setOperationIndex(index);
                reprocessCurrentFrame();
            });

    setOperationIndex(0);
}

void VideoTransformNode::addBaseWidgetPages()
{
    m_stack->addWidget(createInfoPage(
        tr("Swaps the red and blue channels (R<->B) on every frame.")));
    m_stack->addWidget(createInfoPage(tr("Converts every frame to grayscale.")));
    m_stack->addWidget(createInfoPage(tr("Inverts the colors of every frame.")));
    m_stack->addWidget(createSliderPage(m_brightnessSlider, m_brightnessValue, -100, 100,
                                        m_params.brightness, tr("Brightness (-100..+100)"),
                                        [this](int value) {
                                            m_params.brightness = value;
                                            reprocessCurrentFrame();
                                        }));
    m_stack->addWidget(createSliderPage(m_contrastSlider, m_contrastValue, 0, 200,
                                        m_params.contrast, tr("Contrast (0..200%, 100% = unchanged)"),
                                        [this](int value) {
                                            m_params.contrast = value;
                                            reprocessCurrentFrame();
                                        }));
    m_stack->addWidget(createSliderPage(m_blurSlider, m_blurValue, 0, 10,
                                        m_params.blurRadius, tr("Blur radius (0..10)"),
                                        [this](int value) {
                                            m_params.blurRadius = value;
                                            reprocessCurrentFrame();
                                        }));
    m_stack->addWidget(createFlipPage());
    m_stack->addWidget(createInfoPage(tr("Applies a sepia tone to every frame.")));
}

void VideoTransformNode::addOpenCVWidgetPages()
{
#ifdef HAVE_OPENCV
    m_stack->addWidget(createGaussianPage());
    m_stack->addWidget(createCannyPage());
    m_stack->addWidget(createSliderPage(m_thresholdSlider, m_thresholdValue, 0, 255,
                                        m_params.thresholdValue, tr("Threshold value (0..255)"),
                                        [this](int value) {
                                            m_params.thresholdValue = value;
                                            reprocessCurrentFrame();
                                        }));
#endif
}

QWidget *VideoTransformNode::createInfoPage(const QString &text)
{
    auto *page = new QWidget(m_stack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *label = new QLabel(text, page);
    label->setWordWrap(true);
    layout->addWidget(label);

    return page;
}

QWidget *VideoTransformNode::createSliderPage(QSlider *&sliderOut, QLabel *&valueLabelOut,
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

QWidget *VideoTransformNode::createFlipPage()
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
QWidget *VideoTransformNode::createGaussianPage()
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

QWidget *VideoTransformNode::createCannyPage()
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

QImage VideoTransformNode::applyCurrentOperation(const QImage &source) const
{
    if (m_operations.isEmpty())
        return source;
    const int index = std::max(0, std::min(m_operationIndex, static_cast<int>(m_operations.size()) - 1));
    return m_operations[index].apply(source, m_params);
}

void VideoTransformNode::reprocessCurrentFrame()
{
    if (!m_lastInput || m_lastInput->isEmpty())
        return;

    const QImage transformed = applyCurrentOperation(m_lastInput->image());
    if (transformed.isNull())
        return;

    m_output = std::make_shared<ImageData>(transformed);
    Q_EMIT dataUpdated(0);
}

void VideoTransformNode::setOperationIndex(int index)
{
    if (index < 0 || index >= m_operations.size())
        index = 0;

    m_operationIndex = index;

    if (m_operationCombo && m_operationCombo->currentIndex() != index) {
        const QSignalBlocker blocker(m_operationCombo);
        m_operationCombo->setCurrentIndex(index);
    }
    if (m_stack && m_stack->currentIndex() != index)
        m_stack->setCurrentIndex(index);
}

void VideoTransformNode::syncWidgetsFromParams()
{
    if (m_brightnessSlider)
        m_brightnessSlider->setValue(m_params.brightness);
    if (m_contrastSlider)
        m_contrastSlider->setValue(m_params.contrast);
    if (m_blurSlider)
        m_blurSlider->setValue(m_params.blurRadius);
    if (m_flipCombo)
        m_flipCombo->setCurrentIndex(m_params.flipHorizontal ? 0 : 1);
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
