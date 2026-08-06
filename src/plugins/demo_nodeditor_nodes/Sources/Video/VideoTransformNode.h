#ifndef VIDEOTRANSFORMNODE_H
#define VIDEOTRANSFORMNODE_H

#include <QtNodes/NodeDelegateModel>

#include <QImage>
#include <QVector>

#include <functional>
#include <memory>

class QComboBox;
class QLabel;
class QSlider;
class QStackedWidget;
class QWidget;

class ImageData;

/**
 * @brief Video transform node: applies a configurable operation to ImageData frames.
 *
 * Replaces the old fixed VideoModifierNode (R<->B swap only) with a general
 * transform node: 8 base QImage operations are always available (RGB Channel
 * Swap, Grayscale, Invert, Brightness, Contrast, Blur, Flip, Sepia); 3
 * additional OpenCV operations (GaussianBlur, Canny, Threshold) are appended
 * when HAVE_OPENCV is defined (compile-time auto-detect in CMakeLists.txt).
 *
 * The embedded widget is an operation combo + a QStackedWidget with one
 * parameter page per operation. The current operation and its parameters are
 * persisted via save()/load().
 */
class VideoTransformNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    VideoTransformNode();
    ~VideoTransformNode() override;

    QString caption() const override
    { return QStringLiteral("Video Transform"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("VideoTransform"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

private:
    struct TransformParams
    {
        int brightness = 0;
        int contrast = 100;
        int blurRadius = 0;
        bool flipHorizontal = true;
        int gaussianKernel = 5;
        int cannyLow = 50;
        int cannyHigh = 150;
        int thresholdValue = 128;
    };

    struct Operation
    {
        QString id;
        QString displayName;
        std::function<QImage(const QImage &, const TransformParams &)> apply;
    };

    QVector<Operation> buildOperations() const;
    int indexOfOperation(const QString &id) const;

    void buildWidget();
    void addBaseWidgetPages();
    void addOpenCVWidgetPages();
    QWidget *createInfoPage(const QString &text);
    QWidget *createSliderPage(QSlider *&sliderOut, QLabel *&valueLabelOut,
                              int min, int max, int initial, const QString &title,
                              std::function<void(int)> onChanged);
    QWidget *createFlipPage();
#ifdef HAVE_OPENCV
    QWidget *createGaussianPage();
    QWidget *createCannyPage();
#endif

    QImage applyCurrentOperation(const QImage &source) const;
    void reprocessCurrentFrame();
    void setOperationIndex(int index);
    void syncWidgetsFromParams();

    QWidget *m_widget = nullptr;
    QComboBox *m_operationCombo = nullptr;
    QStackedWidget *m_stack = nullptr;

    QSlider *m_brightnessSlider = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QLabel *m_contrastValue = nullptr;
    QSlider *m_blurSlider = nullptr;
    QLabel *m_blurValue = nullptr;
    QComboBox *m_flipCombo = nullptr;

#ifdef HAVE_OPENCV
    QSlider *m_gaussianSlider = nullptr;
    QLabel *m_gaussianValue = nullptr;
    QSlider *m_cannyLowSlider = nullptr;
    QLabel *m_cannyLowValue = nullptr;
    QSlider *m_cannyHighSlider = nullptr;
    QLabel *m_cannyHighValue = nullptr;
    QSlider *m_thresholdSlider = nullptr;
    QLabel *m_thresholdValue = nullptr;
#endif

    QVector<Operation> m_operations;
    int m_operationIndex = 0;
    TransformParams m_params;
    std::shared_ptr<ImageData> m_lastInput;
    std::shared_ptr<ImageData> m_output;
};

#endif // VIDEOTRANSFORMNODE_H
