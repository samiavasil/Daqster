#ifndef VIDEOEFFECTNODE_H
#define VIDEOEFFECTNODE_H

#include "VideoEffectGLProcessor.h"
#include "VideoEffectOps.h"

#include <QtNodes/NodeDelegateModel>

#include <QImage>
#include <QJsonObject>

#include <functional>
#include <memory>

class QComboBox;
class QLabel;
class QSlider;
class QWidget;

class VideoFrameData;

/**
 * @brief Video effect node (REQ-SW-PL-028): one node = one effect.
 *
 * Accepts VideoFrameData on port 0 and emits VideoFrameData on port 0. The
 * backend is chosen at runtime per effect + GL detection:
 *   - CpuOnly effects always run on the CPU (VideoCompat::frameToImage ->
 *     EffectSpec::cpuApply -> QVideoFrame).
 *   - GpuOrCpu effects run on the GPU (VideoEffectGLProcessor) when hardware
 *     GL is available and fall back to the CPU otherwise (including when the
 *     frame format is not NV12/YUV420P).
 *
 * The embedded widget is a single parameter page matching the effect
 * (brightness slider, contrast slider, flip combo, or an info label).
 * Parameters are persisted via save()/load().
 */
class VideoEffectNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    explicit VideoEffectNode(const EffectSpec &spec);
    ~VideoEffectNode() override;

    QString caption() const override;
    bool captionVisible() const override
    { return true; }

    QString name() const override
    { return QStringLiteral("VideoEffect"); }

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
    void buildWidget();
    QWidget *createSliderPage(QSlider *&sliderOut, QLabel *&valueLabelOut,
                              int min, int max, int initial, const QString &title,
                              std::function<void(int)> onChanged);
    QWidget *createFlipPage();
    void syncWidgetsFromParams();
    void reprocessCurrentFrame();
    QImage applyCpu(const QImage &source) const;

    EffectSpec m_spec;
    EffectParams m_params;
    VideoEffectGLProcessor m_glProcessor;

    std::shared_ptr<VideoFrameData> m_lastInput;
    std::shared_ptr<VideoFrameData> m_output;

    QWidget *m_widget = nullptr;
    QSlider *m_brightnessSlider = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QLabel *m_contrastValue = nullptr;
    QComboBox *m_flipCombo = nullptr;
};

// ── 7 thin subclasses — one per effect (REQ-SW-PL-028 AC 4) ──────────────────
// Same pattern as the saved-graph aliases in DemoNodeEditorNodesObject.cpp:
// no Q_OBJECT (no extra signals/slots), the constructor fixes the effect spec
// and name() returns the registry key.
class VideoEffectBrightnessNode : public VideoEffectNode
{
public:
    VideoEffectBrightnessNode()
        : VideoEffectNode(VideoEffectOps::specFor(QStringLiteral("brightness"))) {}
    QString name() const override
    { return QStringLiteral("VideoEffectBrightness"); }
};

class VideoEffectContrastNode : public VideoEffectNode
{
public:
    VideoEffectContrastNode()
        : VideoEffectNode(VideoEffectOps::specFor(QStringLiteral("contrast"))) {}
    QString name() const override
    { return QStringLiteral("VideoEffectContrast"); }
};

class VideoEffectGrayscaleNode : public VideoEffectNode
{
public:
    VideoEffectGrayscaleNode()
        : VideoEffectNode(VideoEffectOps::specFor(QStringLiteral("grayscale"))) {}
    QString name() const override
    { return QStringLiteral("VideoEffectGrayscale"); }
};

class VideoEffectInvertNode : public VideoEffectNode
{
public:
    VideoEffectInvertNode()
        : VideoEffectNode(VideoEffectOps::specFor(QStringLiteral("invert"))) {}
    QString name() const override
    { return QStringLiteral("VideoEffectInvert"); }
};

class VideoEffectSepiaNode : public VideoEffectNode
{
public:
    VideoEffectSepiaNode()
        : VideoEffectNode(VideoEffectOps::specFor(QStringLiteral("sepia"))) {}
    QString name() const override
    { return QStringLiteral("VideoEffectSepia"); }
};

class VideoEffectChannelSwapNode : public VideoEffectNode
{
public:
    VideoEffectChannelSwapNode()
        : VideoEffectNode(VideoEffectOps::specFor(QStringLiteral("channelSwap"))) {}
    QString name() const override
    { return QStringLiteral("VideoEffectChannelSwap"); }
};

class VideoEffectFlipNode : public VideoEffectNode
{
public:
    VideoEffectFlipNode()
        : VideoEffectNode(VideoEffectOps::specFor(QStringLiteral("flip"))) {}
    QString name() const override
    { return QStringLiteral("VideoEffectFlip"); }
};

#endif // VIDEOEFFECTNODE_H