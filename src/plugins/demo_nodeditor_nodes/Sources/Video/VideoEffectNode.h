#ifndef VIDEOEFFECTNODE_H
#define VIDEOEFFECTNODE_H

#include "VideoEffectGLProcessor.h"
#include "VideoEffectOps.h"

#include <QtNodes/NodeDelegateModel>

#include <QImage>
#include <QJsonObject>
#include <QVector>

#include <functional>
#include <memory>

class QComboBox;
class QLabel;
class QSlider;
class QStackedWidget;
class QWidget;

class VideoFrameData;

/**
 * @brief Video effect node (REQ-SW-PL-028): ONE node with an effect combo.
 *
 * Accepts VideoFrameData on port 0 and emits VideoFrameData on port 0. The
 * effect is chosen from a combo box (like VideoTransformNode / the Image
 * path: combo + QStackedWidget); the backend is chosen at runtime per
 * effect + GL detection:
 *   - CpuOnly effects always run on the CPU (asImage() -> EffectSpec::cpuApply
 *     -> QVideoFrame).
 *   - GpuOrCpu effects run on the GPU (VideoEffectGLProcessor) when hardware
 *     GL is available and fall back to the CPU otherwise (including when the
 *     frame format is not NV12/YUV420P).
 *
 * The embedded widget is an effect combo + a QStackedWidget with one
 * parameter page per effect (brightness slider, contrast slider, flip combo,
 * or an info label). Parameters are persisted via save()/load() — the format
 * is backward compatible with the old per-effect subclasses ("effect" = id +
 * parameters).
 *
 * The 7 old per-effect subclasses (VideoEffectBrightnessNode, ...) remain as
 * deprecated aliases so old saved graphs keep working (Task 3.2).
 */
class VideoEffectNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    VideoEffectNode();
    ~VideoEffectNode() override;

    QString caption() const override
    { return QStringLiteral("Video Effect"); }

    bool captionVisible() const override
    { return true; }

    QString name() const override
    { return QStringLiteral("VideoEffect"); }

    /// Selects the effect by id (from VideoEffectOps::allSpecs()). Unknown
    /// ids fall back to index 0. Syncs the widget stack and reprocesses the
    /// current frame.
    void setEffect(const QString &id);

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
    int indexOfEffect(const QString &id) const;
    void setEffectIndex(int index);
    void buildWidget();
    QWidget *createInfoPage(const QString &text);
    QWidget *createSliderPage(QSlider *&sliderOut, QLabel *&valueLabelOut,
                              int min, int max, int initial, const QString &title,
                              std::function<void(int)> onChanged);
    QWidget *createFlipPage();
    void syncWidgetsFromParams();
    void reprocessCurrentFrame();
    QImage applyCpu(const QImage &source) const;

    QVector<EffectSpec> m_specs;
    int m_effectIndex = 0;
    EffectParams m_params;
    VideoEffectGLProcessor m_glProcessor;

    std::shared_ptr<VideoFrameData> m_lastInput;
    std::shared_ptr<VideoFrameData> m_output;

    QWidget *m_widget = nullptr;
    QComboBox *m_effectCombo = nullptr;
    QStackedWidget *m_stack = nullptr;
    QSlider *m_brightnessSlider = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QLabel *m_contrastValue = nullptr;
    QComboBox *m_flipCombo = nullptr;
};

// ── 7 deprecated aliases — one per effect (REQ-SW-PL-028 AC 4) ───────────────
// Old saved graphs reference these registry keys ("VideoEffectBrightness",
// "VideoEffectContrast", ...). Each alias is a plain VideoEffectNode with the
// effect preselected via setEffect(); no Q_OBJECT (no extra signals/slots).
// New graphs should use the single "VideoEffect" node with the effect combo.
class VideoEffectBrightnessNode : public VideoEffectNode
{
public:
    VideoEffectBrightnessNode() : VideoEffectNode() { setEffect(QStringLiteral("brightness")); }
    QString name() const override
    { return QStringLiteral("VideoEffectBrightness"); }
};

class VideoEffectContrastNode : public VideoEffectNode
{
public:
    VideoEffectContrastNode() : VideoEffectNode() { setEffect(QStringLiteral("contrast")); }
    QString name() const override
    { return QStringLiteral("VideoEffectContrast"); }
};

class VideoEffectGrayscaleNode : public VideoEffectNode
{
public:
    VideoEffectGrayscaleNode() : VideoEffectNode() { setEffect(QStringLiteral("grayscale")); }
    QString name() const override
    { return QStringLiteral("VideoEffectGrayscale"); }
};

class VideoEffectInvertNode : public VideoEffectNode
{
public:
    VideoEffectInvertNode() : VideoEffectNode() { setEffect(QStringLiteral("invert")); }
    QString name() const override
    { return QStringLiteral("VideoEffectInvert"); }
};

class VideoEffectSepiaNode : public VideoEffectNode
{
public:
    VideoEffectSepiaNode() : VideoEffectNode() { setEffect(QStringLiteral("sepia")); }
    QString name() const override
    { return QStringLiteral("VideoEffectSepia"); }
};

class VideoEffectChannelSwapNode : public VideoEffectNode
{
public:
    VideoEffectChannelSwapNode() : VideoEffectNode() { setEffect(QStringLiteral("channelSwap")); }
    QString name() const override
    { return QStringLiteral("VideoEffectChannelSwap"); }
};

class VideoEffectFlipNode : public VideoEffectNode
{
public:
    VideoEffectFlipNode() : VideoEffectNode() { setEffect(QStringLiteral("flip")); }
    QString name() const override
    { return QStringLiteral("VideoEffectFlip"); }
};

#endif // VIDEOEFFECTNODE_H