#ifndef VIDEOEFFECTNODE_H
#define VIDEOEFFECTNODE_H

#include "VideoEffectGLProcessor.h"
#include "VideoEffectOps.h"

#include <QtNodes/NodeDelegateModel>

#include <QByteArray>
#include <QImage>
#include <QJsonObject>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

class QComboBox;
class QLabel;
class QSlider;
class QStackedWidget;
class QTimer;
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
 * Threading (REQ-SW-PL-039): the GPU path stays on the GUI thread (GL-bound).
 * The CPU path snapshots the input on the GUI thread (a cheap implicit-share
 * QVideoFrame copy for CPU-resident frames, or the asImage() readback for
 * GpuRgba frames) and submits the applyCpu() pass to the shared ComputePool
 * under a per-node key — the pool's per-key "latest-wins" submission skips
 * stale frames while a task runs. The worker converts its OWN frame copy
 * (never the shared VideoFrameData) and posts the result back via a queued
 * onCpuResult() slot. The widget shows a small CPU metric label (completed /
 * submitted / skipped / fps) refreshed on each result.
 *
 * The embedded widget is an effect combo + a QStackedWidget with one
 * parameter page per effect (brightness slider, contrast slider, flip combo,
 * or an info label). Parameters are persisted via save()/load() — the format
 * is backward compatible with the old per-effect subclasses ("effect" = id +
 * parameters).
 *
 * The 7 old per-effect subclasses (VideoEffectBrightnessNode, ...) were
 * removed on 2026-08-26 (user decision) — old saved graphs that reference the
 * alias registry keys no longer load; the single "VideoEffect" node is the
 * only registered effect node.
 */
class VideoEffectNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

    friend class VideoEffectNodeTest;

public:
    VideoEffectNode();
    ~VideoEffectNode() override;

    QString caption() const override
    { return QStringLiteral("Video Effect"); }

    bool captionVisible() const override
    { return true; }

    QString name() const override
    { return QStringLiteral("VideoEffect"); }

    /// Video nodes do not change their geometry on data arrival — the display
    /// is updated directly in setInData(). Opts out of the full scene geometry
    /// recompute cascade (repaint-only fast path on data arrival).
    bool dataArrivalChangesGeometry() const override { return false; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. Opts out of the body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

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

private slots:
    /// GUI-thread result delivery for the CPU path (Qt::QueuedConnection from
    /// the ComputePool worker). Sets m_output and emits dataUpdated(0).
    void onCpuResult(QImage result);

    /// Optional `[PERF] effect` console line (5 s timer, mirrors
    /// VideoOutputNode::logPerfLine) — only prints when the "video" perf
    /// domain is enabled.
    void logPerfLine();

private:
    int indexOfEffect(const QString &id) const;
    void setEffectIndex(int index);
    void buildWidget();
    QWidget *createInfoPage(const QString &text);
    QWidget *createSliderPage(QSlider *&sliderOut, QLabel *&valueLabelOut,
                              int min, int max, int initial, const QString &title,
                              std::function<void(int)> onChanged);
    QWidget *createFlipPage();
#ifdef HAVE_OPENCV
    QWidget *createGaussianPage();
    QWidget *createCannyPage();
#endif
    void syncWidgetsFromParams();
    void reprocessCurrentFrame();
    /// Pure CPU effect pass — reads ONLY the passed spec/params (never node
    /// members), so it is safe to call from a ComputePool worker thread.
    QImage applyCpu(const QImage &source, const EffectSpec &spec,
                    const EffectParams &params) const;
    /// Refresh the CPU metric label from the pool's per-key counters.
    void updateMetricLabel();

    QVector<EffectSpec> m_specs;
    int m_effectIndex = 0;
    EffectParams m_params;
    VideoEffectGLProcessor m_glProcessor;

    std::shared_ptr<VideoFrameData> m_lastInput;
    std::shared_ptr<VideoFrameData> m_output;

    // ── ComputePool CPU path (REQ-SW-PL-039) ────────────────────────────────
    std::atomic<bool> m_shuttingDown{false};
    QByteArray m_poolKey;               // per-node key into the shared pool
    quint64 m_totalFrames = 0;          // all valid setInData calls
    QTimer *m_perfTimer = nullptr;      // 5 s [PERF] effect console line

    QWidget *m_widget = nullptr;
    QComboBox *m_effectCombo = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_metricLabel = nullptr;    // CPU metric label below the stack
    QSlider *m_brightnessSlider = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QLabel *m_contrastValue = nullptr;
    QComboBox *m_flipCombo = nullptr;
    QSlider *m_blurSlider = nullptr;
    QLabel *m_blurValue = nullptr;
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
};

#endif // VIDEOEFFECTNODE_H
