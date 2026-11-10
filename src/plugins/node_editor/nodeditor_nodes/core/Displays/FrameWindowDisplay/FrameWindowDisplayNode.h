#pragma once

#include "nodeditor_nodes_core_export.h"

#include "shared/IStoppable.h"

#include <QtNodes/NodeDelegateModel>

#include <QElapsedTimer>
#include <QImage>
#include <QMetaType>
#include <QString>
#include <QVector>

#include <memory>

class DisplayRefreshDriver;
class FrameWindowData;
class QWidget;
class FrameWindowDisplayNode;

/**
 * @brief What one off-GUI pass found in the frame window (REQ-SW-PL-090).
 *
 * `thumbnails` holds at most kMaxThumbnails DOWNSCALED images built with
 * VideoFrameData::frameToImageScaled() on the worker thread — the full frame is
 * never materialized and never retained. `skipped` counts the frames that could
 * not be turned into a thumbnail (GPU-resident payloads, whose readback is
 * GUI-thread only, and unreadable pixel formats); the metadata stays truthful
 * about them instead of showing an empty box as "no frames".
 */
struct FrameWindowDisplaySnapshot
{
    int count = 0;                    ///< frames in the window
    qint64 firstTimestampMs = -1;     ///< arrival time of the oldest frame
    qint64 lastTimestampMs = -1;      ///< arrival time of the newest frame
    qint64 spanMs = 0;                ///< last - first (0 for a single frame)
    QVector<QImage> thumbnails;       ///< ≤ kMaxThumbnails, oldest first
    int skipped = 0;                  ///< frames without a thumbnail
    qint64 messages = 0;              ///< FrameWindowData blocks received
    qint64 msSinceLast = -1;          ///< -1 until the first block arrives
    bool metadataOnly = false;        ///< no thumbnail is available at all
};
Q_DECLARE_METATYPE(FrameWindowDisplaySnapshot)

/// GUI-thread bridge for queued window snapshots (queued delivery only).
class FrameWindowDisplayBridge : public QObject
{
    Q_OBJECT

public:
    explicit FrameWindowDisplayBridge(FrameWindowDisplayNode *node)
        : m_node(node)
    {
    }

public slots:
    void onComputeDone(FrameWindowDisplaySnapshot snapshot);

private:
    FrameWindowDisplayNode *m_node;
};

/**
 * @brief Frame-window display node — 1 input, 0 outputs (REQ-SW-PL-090).
 *
 * Shows a `FrameWindowData` block: how many frames it carries, the arrival
 * timestamps of the first and the last one (and their span), plus thumbnails of
 * up to four frames. FrameWindowData holds `std::shared_ptr<VideoFrameData>`
 * items, so the window itself is a list of pointers — the node forwards those
 * pointers to the worker and never touches a pixel on the graph thread.
 *
 * Thumbnails use the pure-CPU, subsampled VideoFrameData::frameToImageScaled()
 * (REQ-SW-PL-053), which is safe from any thread. A GPU-resident frame
 * (isGpuResident()) is deliberately NOT converted: its only CPU path is
 * asImage(), which does a GL readback and is GUI-thread only — converting it
 * here would violate the threading contract, so it is reported as skipped.
 *
 * REQ-SW-PL-051: no widget — embeddedWidget() returns nullptr;
 * FrameWindowDisplayWidget lives in the GUI plugin.
 */
class NODEDITOR_NODES_CORE_EXPORT FrameWindowDisplayNode : public QtNodes::NodeDelegateModel,
                                                           public Daqster::IStoppable
{
    Q_OBJECT

    friend class TestDisplayNodes;

public:
    FrameWindowDisplayNode();
    ~FrameWindowDisplayNode() override;

    QString caption() const override
    { return QStringLiteral("Frame Window Display"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("FrameWindowDisplay"); }

    bool dataArrivalChangesWidget() const override { return false; }

    bool dataArrivalChangesGeometry() const override { return false; }

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    /// A display is a consumer: 1 input, 0 outputs — always nullptr.
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override { return nullptr; }

    /// Stop the refresh timer and cancel queued work. Idempotent (REQ-SW-PL-050).
    void stop() override;

    /// 2×2 thumbnails in the node body.
    static constexpr int kMaxThumbnails = 4;
    /// Thumbnail width budget — the subsampled converter never builds a bigger
    /// image, so the cost is (thumb/src)² of a full conversion.
    static constexpr int kThumbnailWidth = 72;

    /// Read-only state (tests).
    qint64 messageCount() const { return m_messages; }

    /// Pure worker body: window metadata + ≤ maxThumbnails thumbnails. Const
    /// access only — the window's frames are shared, never written.
    static FrameWindowDisplaySnapshot buildSnapshot(const FrameWindowData &window,
                                                     qint64 messages,
                                                     qint64 msSinceLast,
                                                     int maxThumbnails = kMaxThumbnails,
                                                     int thumbnailWidth = kThumbnailWidth);

signals:
    void snapshotReady(FrameWindowDisplaySnapshot const &snapshot);

private slots:
    void onRefreshRequested();

private:
    DisplayRefreshDriver *m_driver = nullptr;
    FrameWindowDisplayBridge *m_bridge = nullptr;

    /// Newest window only. FrameWindowData carries shared_ptr items, so this
    /// hold costs N reference counts — no pixels are copied, and it is released
    /// as soon as the pass has produced its thumbnails.
    std::shared_ptr<const FrameWindowData> m_lastWindow;

    qint64 m_messages = 0;
    QElapsedTimer m_clock;
    qint64 m_lastArrivalMs = -1;
};
