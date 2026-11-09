#ifndef FRAMEWINDOWNODE_H
#define FRAMEWINDOWNODE_H

#include "nodeditor_nodes_core_export.h"

#include "NodeDataTypes/FrameWindowData.h"

#include <QtNodes/NodeDelegateModel>

#include <QElapsedTimer>
#include <QJsonObject>
#include <QVector>

#include <memory>

class QWidget;

/**
 * @brief Input-side video sampling: publishes the last N frames as one block
 * (REQ-SW-PL-086).
 *
 * Accepts VideoFrameData on port 0 and emits FrameWindowData on port 0. The
 * node keeps a sliding window of the last `window` frames (2…32) together with
 * their arrival timestamps, so an omni/vision consumer receives the whole
 * window in one call instead of collecting frames itself.
 *
 * Emit modes:
 *   - Full:       emit only once `window` frames are collected, then keep
 *                 sliding and emit on every new frame.
 *   - EveryFrame: emit the current (partial) window on every new frame.
 *   - Interval:   emit at most once per `intervalMs` (the first frame emits
 *                 immediately, then the gate opens again after the interval).
 *
 * Zero-copy: the window holds the same std::shared_ptr<VideoFrameData> objects
 * the input produced — no pixel copies, no asImage() conversion.
 *
 * A null/non-video input clears the window and emits dataInvalidated(0).
 *
 * REQ-SW-PL-051: this model owns NO widgets. The controls live in
 * FrameWindowWidget (GUI plugin) and are created through NodeWidgetFactory.
 */
class NODEDITOR_NODES_CORE_EXPORT FrameWindowNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    FrameWindowNode();
    ~FrameWindowNode() override;

    QString caption() const override
    { return QStringLiteral("Frame Window"); }

    bool captionVisible() const override
    { return true; }

    QString name() const override
    { return QStringLiteral("FrameWindow"); }

    /// Video nodes do not change their geometry on data arrival.
    bool dataArrivalChangesGeometry() const override { return false; }

    /// The node body does not depend on data — the widget self-repaints.
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    /// Core model has no QtWidgets dependency (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Window emit policy, shared with the widget's mode selector.
    enum class EmitMode { Full, EveryFrame, Interval };

    // ── State read by the GUI widget ───────────────────────────────────────
    EmitMode emitMode() const { return m_emitMode; }
    int windowSize() const { return m_window; }
    int intervalMs() const { return m_intervalMs; }

    /// Frames currently held in the sliding window.
    int collectedCount() const { return m_items.size(); }

signals:
    /// A parameter changed (or load() restored one) — re-render the controls.
    void paramsChanged(int mode, int windowSize, int intervalMs);

public slots:
    /// Mode selector changed.
    void onModeChanged(int mode);
    /// "Window =" spin box changed (clamped to 2…32).
    void onWindowChanged(int windowSize);
    /// "Interval ms =" spin box changed (clamped to 1…60000).
    void onIntervalMsChanged(int intervalMs);

private:
    /// Trims the window to the configured size (used after a size change).
    void trimWindow();
    /// True when the emit gate allows publishing the current window.
    bool shouldEmit() const;
    /// Publishes the current window as one FrameWindowData block.
    void emitWindow();

    EmitMode m_emitMode = EmitMode::Full;
    int m_window = 4;
    int m_intervalMs = 1000;

    QVector<FrameWindowData::Item> m_items;
    std::shared_ptr<FrameWindowData> m_output;

    /// Monotonic arrival clock (started in the constructor).
    QElapsedTimer m_clock;
    /// Last emit time on m_clock, or -1 when nothing has been emitted yet.
    qint64 m_lastEmitMs = -1;
};

#endif // FRAMEWINDOWNODE_H
