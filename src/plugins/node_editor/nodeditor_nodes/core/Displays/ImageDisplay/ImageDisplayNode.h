#pragma once

#include "nodeditor_nodes_core_export.h"

#include "shared/IStoppable.h"

#include <QtNodes/NodeDelegateModel>

#include <QElapsedTimer>
#include <QImage>
#include <QMetaType>
#include <QSize>
#include <QString>

#include <memory>

class DisplayRefreshDriver;
class ImageData;
class QWidget;
class ImageDisplayNode;

/**
 * @brief What one off-GUI pass found in the image stream (REQ-SW-PL-090).
 *
 * `thumbnail` is a DOWNSCALED copy built on the worker thread: the display
 * never keeps the producer's QImage alive, so a 4K frame is not pinned by an
 * idle node. The full-resolution image stays with its producer.
 */
struct ImageDisplaySnapshot
{
    QImage thumbnail;        ///< bounded preview (≤ kPreviewWidth × kPreviewHeight)
    int width = 0;           ///< source width, before downscaling
    int height = 0;          ///< source height, before downscaling
    QString format;          ///< source QImage::Format as text
    QString mimeType;        ///< ImageData::Meta::mimeType (may be empty)
    QString model;           ///< ImageData::Meta::model (may be empty)
    qint64 metaElapsedMs = 0;///< ImageData::Meta::elapsedMs — producer's own timing
    qint64 messages = 0;     ///< ImageData blocks received so far
    qint64 msSinceLast = -1; ///< -1 until the first block arrives
    bool downscaled = false; ///< the preview is smaller than the source
};
Q_DECLARE_METATYPE(ImageDisplaySnapshot)

/// GUI-thread bridge for queued image snapshots (queued delivery only).
class ImageDisplayBridge : public QObject
{
    Q_OBJECT

public:
    explicit ImageDisplayBridge(ImageDisplayNode *node)
        : m_node(node)
    {
    }

public slots:
    void onComputeDone(ImageDisplaySnapshot snapshot);

private:
    ImageDisplayNode *m_node;
};

/**
 * @brief Image display node — 1 input, 0 outputs (REQ-SW-PL-090).
 *
 * Shows the last `ImageData` of the graph scaled to fit (plus its size, pixel
 * format and age). The scaling runs on the shared ComputePool: `setInData()`
 * on the graph thread only bumps the shared_ptr and raises a flag.
 *
 * Const access only — `ImageData::image()` returns a const reference and no
 * pass ever writes into it. The QImage is converted on a worker thread, which
 * is safe because QImage value operations on an immutable source touch no GL
 * and no RHI (contrast: VideoFrameData::asImage() is GUI-thread only).
 *
 * REQ-SW-PL-051: no widget — embeddedWidget() returns nullptr;
 * ImageDisplayWidget lives in the GUI plugin.
 */
class NODEDITOR_NODES_CORE_EXPORT ImageDisplayNode : public QtNodes::NodeDelegateModel,
                                                     public Daqster::IStoppable
{
    Q_OBJECT

    friend class TestDisplayNodes;

public:
    ImageDisplayNode();
    ~ImageDisplayNode() override;

    QString caption() const override
    { return QStringLiteral("Image Display"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("ImageDisplay"); }

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

    /// Preview bounds — one thumbnail fits the node body without scrolling.
    static constexpr int kPreviewWidth = 320;
    static constexpr int kPreviewHeight = 180;

    /// Read-only state (tests).
    qint64 messageCount() const { return m_messages; }
    QSize lastSize() const { return m_lastSize; }

    /// QImage::Format → stable text (the enum values differ between Qt 5/6,
    /// so the NUMBER is never shown — only this name).
    static QString formatName(QImage::Format format);

    /// Pure worker body: downscale `image` and collect the metadata. Never
    /// touches the source image's pixels beyond reading them.
    static ImageDisplaySnapshot buildSnapshot(const QImage &image,
                                              const QString &mimeType,
                                              const QString &model,
                                              qint64 metaElapsedMs,
                                              qint64 messages,
                                              qint64 msSinceLast,
                                              int maxWidth = kPreviewWidth,
                                              int maxHeight = kPreviewHeight);

signals:
    void snapshotReady(ImageDisplaySnapshot const &snapshot);

private slots:
    void onRefreshRequested();

private:
    DisplayRefreshDriver *m_driver = nullptr;
    ImageDisplayBridge *m_bridge = nullptr;

    /// Newest payload only; released as soon as the pass has built its
    /// thumbnail (the display does not need the full image between passes).
    std::shared_ptr<const ImageData> m_lastData;
    QSize m_lastSize;

    qint64 m_messages = 0;
    QElapsedTimer m_clock;
    qint64 m_lastArrivalMs = -1;
};
