#pragma once

#include "nodeditor_nodes_core_export.h"

#include "shared/IStoppable.h"

#include <QtNodes/NodeDelegateModel>

#include <QElapsedTimer>
#include <QMetaType>
#include <QString>

#include <memory>

class DisplayRefreshDriver;
class QWidget;
class TextDisplayNode;

/**
 * @brief What one off-GUI pass found in the text stream (REQ-SW-PL-090).
 *
 * The worker builds this from the last payload; the widget only renders it.
 * `preview` is truncated to `kPreviewChars`, `full` is what the tooltip shows —
 * that is the only reason the node keeps the text at all.
 */
struct TextDisplaySnapshot
{
    QString preview;          ///< truncated, single-screen text
    QString full;             ///< the whole received text (tooltip)
    qint64 messages = 0;      ///< TextData blocks received so far
    qint64 characters = 0;    ///< sum of every received text length
    qint64 msSinceLast = -1;  ///< -1 until the first block arrives
    int lines = 0;            ///< line count of the FULL text
    bool truncated = false;   ///< preview dropped characters
};
Q_DECLARE_METATYPE(TextDisplaySnapshot)

/**
 * @brief GUI-thread bridge for queued text snapshots.
 *
 * Lives on the GUI thread. Worker tasks post here with Qt::QueuedConnection;
 * the slot only forwards to the node's signal (no formatting, no I/O).
 */
class TextDisplayBridge : public QObject
{
    Q_OBJECT

public:
    explicit TextDisplayBridge(TextDisplayNode *node)
        : m_node(node)
    {
    }

public slots:
    void onComputeDone(TextDisplaySnapshot snapshot);

private:
    TextDisplayNode *m_node;
};

/**
 * @brief Text display node — 1 input, 0 outputs (REQ-SW-PL-090).
 *
 * The "last text" consumer of the graph: `TextData` in, nothing out. It shows
 * the newest text (truncated in the preview, whole in the tooltip) plus the
 * stream's counters: total blocks, total characters and how long ago the last
 * block arrived.
 *
 * Threading: `setInData()` only stores the newest QString (implicit sharing —
 * no characters are copied) and raises a flag. A 30 Hz ticket from
 * DisplayRefreshDriver submits ONE pass to the shared ComputePool that builds
 * the snapshot; the result comes back through TextDisplayBridge with a queued
 * invocation. Nothing here blocks the graph thread.
 *
 * The node never mutates the received TextData: it reads `text()` and lets the
 * shared_ptr go.
 *
 * REQ-SW-PL-051: no widget — embeddedWidget() returns nullptr and the GUI
 * plugin builds TextDisplayWidget through NodeWidgetFactory.
 */
class NODEDITOR_NODES_CORE_EXPORT TextDisplayNode : public QtNodes::NodeDelegateModel,
                                                    public Daqster::IStoppable
{
    Q_OBJECT

    friend class TestDisplayNodes;

public:
    TextDisplayNode();
    ~TextDisplayNode() override;

    QString caption() const override
    { return QStringLiteral("Text Display"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("TextDisplay"); }

    /// The node body does not depend on data — the widget self-repaints.
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

    /// Characters kept in the preview before the ellipsis.
    static constexpr int kPreviewChars = 240;

    // ── Read-only state (tests + widget tooltips) ──────────────────────────
    qint64 messageCount() const { return m_messages; }
    qint64 characterCount() const { return m_characters; }
    QString lastText() const { return m_lastText; }

    /// Pure snapshot builder — the exact body of the worker pass, so the
    /// truncation and counter rules are testable without a widget or a pool.
    static TextDisplaySnapshot buildSnapshot(const QString &text,
                                             qint64 messages,
                                             qint64 characters,
                                             qint64 msSinceLast,
                                             int previewChars = kPreviewChars);

signals:
    /// A compute pass finished; the widget renders it.
    void snapshotReady(TextDisplaySnapshot const &snapshot);

private slots:
    void onRefreshRequested();

private:
    DisplayRefreshDriver *m_driver = nullptr;
    TextDisplayBridge *m_bridge = nullptr;

    /// Newest text only — implicit sharing, no character copies.
    QString m_lastText;
    qint64 m_messages = 0;
    qint64 m_characters = 0;
    /// Monotonic arrival clock for "ms since the last message".
    QElapsedTimer m_clock;
    qint64 m_lastArrivalMs = -1;
};
