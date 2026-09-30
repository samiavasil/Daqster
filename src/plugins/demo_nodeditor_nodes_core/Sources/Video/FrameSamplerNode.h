#ifndef FRAMESAMPLERNODE_H
#define FRAMESAMPLERNODE_H

#include <QtNodes/NodeDelegateModel>

#include <QElapsedTimer>
#include <QJsonObject>

#include <memory>

class QWidget;

class VideoFrameData;

/**
 * @brief Frame resampling node (REQ-SW-PL-030).
 *
 * Accepts VideoFrameData on port 0 and emits VideoFrameData on port 0, gated
 * by one of two modes:
 *   - EveryNth: pass every N-th incoming frame (N >= 1).
 *   - MaxFps: pass at most `maxFps` frames per second (timer-based).
 *
 * Zero-copy, fan-out: the passed frame is the SAME shared_ptr<VideoFrameData>
 * as the input (ref-count bump only) — no QImage conversion, no frame copy.
 * The node works on the frame, never triggers asImage()/frameToImage().
 *
 * REQ-SW-PL-051: this model owns NO widgets. The mode selector and the two
 * parameter spin boxes live in FrameSamplerWidget (GUI plugin) and are created
 * through NodeWidgetFactory.
 */
class FrameSamplerNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    FrameSamplerNode();
    ~FrameSamplerNode() override;

    QString caption() const override
    { return QStringLiteral("Frame Sampler"); }

    bool captionVisible() const override
    { return true; }

    QString name() const override
    { return QStringLiteral("FrameSampler"); }

    /// Video nodes do not change their geometry on data arrival — the display
    /// is updated directly in setInData(). Opts out of the full scene geometry
    /// recompute cascade (repaint-only fast path on data arrival).
    bool dataArrivalChangesGeometry() const override { return false; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. Opts out of the body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    /// Core model has no QtWidgets dependency — the widget is created by the
    /// GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Frame-gating mode, shared with the widget's mode selector.
    enum class Mode { EveryNth, MaxFps };

    // ── State read by the GUI widget ───────────────────────────────────────
    Mode mode() const { return m_mode; }
    int everyN() const { return m_everyN; }
    int maxFps() const { return m_maxFps; }

signals:
    /// A parameter changed (or load() restored one) — re-render the controls.
    void paramsChanged(int mode, int everyN, int maxFps);

public slots:
    /// Mode selector changed.
    void onModeChanged(int mode);
    /// "N =" spin box changed.
    void onEveryNChanged(int everyN);
    /// "Max FPS =" spin box changed.
    void onMaxFpsChanged(int maxFps);

private:
    void resetGate();
    bool passesGate();

    Mode m_mode = Mode::EveryNth;
    int m_everyN = 2;
    int m_maxFps = 10;
    quint64 m_frameCounter = 0;
    QElapsedTimer m_fpsTimer;

    std::shared_ptr<VideoFrameData> m_lastInput;
    std::shared_ptr<VideoFrameData> m_output;
};

#endif // FRAMESAMPLERNODE_H