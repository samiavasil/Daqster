#pragma once

#include <QtNodes/NodeData>

#include <QVector>

#include <memory>
#include <utility>

#include "VideoFrameData.h"

/**
 * @brief A window of video frames as one graph data type (REQ-SW-PL-086).
 *
 * Input-side sampling: a vision/omni node should not collect frames itself.
 * FrameWindowNode accumulates the last N VideoFrameData objects and publishes
 * them as a single FrameWindowData, so a consumer receives the whole window in
 * one block.
 *
 * Zero-copy: every Item holds the SAME std::shared_ptr<VideoFrameData> the
 * source produced — no pixels are copied, no QImage is materialized. Frames
 * are shared value-free through shared_ptr, so holding N of them only bumps N
 * reference counts.
 *
 * The copy constructor and assignment are deleted for the same reason
 * VideoFrameData deletes them: the payload is meant to be shared, never copied.
 */
class FrameWindowData : public QtNodes::NodeData
{
public:
    /// One entry of the window: the frame plus its arrival time (ms).
    struct Item
    {
        std::shared_ptr<VideoFrameData> frame;
        qint64 timestampMs = 0;
    };

    FrameWindowData() = default;

    explicit FrameWindowData(QVector<Item> items)
        : m_items(std::move(items))
    {}

    FrameWindowData(const FrameWindowData &) = delete;
    FrameWindowData &operator=(const FrameWindowData &) = delete;

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"frame-window", "Frame Window"};
    }

    /// The frames in arrival order (oldest first). Never copied.
    const QVector<Item> &items() const { return m_items; }

    bool isEmpty() const { return m_items.isEmpty(); }

    int count() const { return m_items.size(); }

    /// Arrival timestamp of the oldest frame, or -1 for an empty window.
    qint64 firstTimestampMs() const
    {
        return m_items.isEmpty() ? -1 : m_items.first().timestampMs;
    }

    /// Arrival timestamp of the newest frame, or -1 for an empty window.
    qint64 lastTimestampMs() const
    {
        return m_items.isEmpty() ? -1 : m_items.last().timestampMs;
    }

private:
    QVector<Item> m_items;
};
