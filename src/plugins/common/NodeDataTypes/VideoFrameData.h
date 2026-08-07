#pragma once

#include <QtGlobal>
#include <QtNodes/NodeDelegateModel>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QVideoFrame>
#endif

/**
 * @brief Zero-copy video frame node data type (Qt6).
 *
 * Wraps a Qt6 QVideoFrame and transports it through the graph as
 * std::shared_ptr<NodeData> — only the reference count is bumped, the
 * decoded pixel buffer is never copied. Qt6 QVideoFrame is implicitly
 * shared (QSharedData), so holding and passing it is safe.
 *
 * On Qt5 this type carries no frame (hasFrame() always returns false):
 * Qt5 probe frames are NOT safe to hold beyond the signal (the backend
 * recycles the buffers), so the zero-copy path is Qt6-only and Qt5 callers
 * keep the ImageData/QImage path (REQ-SW-PL-020, AC 1 + AC 5).
 */
class VideoFrameData : public QtNodes::NodeData
{
public:
    VideoFrameData() = default;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    explicit VideoFrameData(const QVideoFrame &frame) : m_frame(frame) {}

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"video-frame", "Video Frame"};
    }

    /// The wrapped frame. Valid only when hasFrame() returns true.
    const QVideoFrame &frame() const { return m_frame; }

    /// Replaces the wrapped frame (ref-count bump only — zero-copy).
    void setFrame(const QVideoFrame &frame) { m_frame = frame; }

    bool hasFrame() const { return m_frame.isValid(); }

private:
    QVideoFrame m_frame;
#else
    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"video-frame", "Video Frame"};
    }

    /// Qt5 probe frames are not safe to hold — never a frame here.
    bool hasFrame() const { return false; }
#endif
};
