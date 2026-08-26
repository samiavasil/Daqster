#ifndef FRAMESAMPLERNODE_H
#define FRAMESAMPLERNODE_H

#include <QtNodes/NodeDelegateModel>

#include <QElapsedTimer>
#include <QJsonObject>

#include <memory>

class QComboBox;
class QSpinBox;
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
    enum class Mode { EveryNth, MaxFps };

    void buildWidget();
    void resetGate();
    bool passesGate();
    void syncWidgetsFromParams();
    void updateSpinVisibility();

    Mode m_mode = Mode::EveryNth;
    int m_everyN = 2;
    int m_maxFps = 10;
    quint64 m_frameCounter = 0;
    QElapsedTimer m_fpsTimer;

    std::shared_ptr<VideoFrameData> m_lastInput;
    std::shared_ptr<VideoFrameData> m_output;

    QWidget *m_widget = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QSpinBox *m_everyNSpin = nullptr;
    QSpinBox *m_maxFpsSpin = nullptr;
};

#endif // FRAMESAMPLERNODE_H