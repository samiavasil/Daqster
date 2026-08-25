#pragma once

#include <QtGlobal>
#include <QtNodes/NodeDelegateModel>

#include <QImage>
#include <QtMultimedia/QVideoFrame>

/**
 * @brief Zero-copy video frame node data type.
 *
 * Wraps a QVideoFrame and transports it through the graph as
 * std::shared_ptr<NodeData> — only the reference count is bumped, the
 * decoded pixel buffer is never copied. QVideoFrame is implicitly shared
 * (QSharedData), so holding and passing it is safe.
 *
 * On Qt6 the wrapped frame is the decoded probe frame (ref-count bump only).
 * On Qt5 probe frames are NOT safe to hold beyond the signal (the backend
 * recycles the buffers), so sources wrap an OWNED copy instead: the planes
 * are memcpy'd into QByteArray storage via VideoCompat::frameToOwnedFrame()
 * (NV12 / YUV420P; unsupported formats stay on the QImage path). The
 * transport through the graph is then zero-copy on both versions — the copy
 * happens exactly once in the source (REQ-SW-PL-020, NV12-direct for Qt5).
 */
class VideoFrameData : public QtNodes::NodeData
{
public:
    VideoFrameData() = default;

    explicit VideoFrameData(const QVideoFrame &frame) : m_frame(frame) {}

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"video-frame", "Video Frame"};
    }

    /// The wrapped frame. Valid only when hasFrame() returns true.
    const QVideoFrame &frame() const { return m_frame; }

    /// Replaces the wrapped frame (ref-count bump only — zero-copy).
    void setFrame(const QVideoFrame &frame)
    {
        m_frame = frame;
        // A new frame invalidates the lazy QImage cache (REQ-SW-PL-032).
        m_imageCache = QImage();
    }

    bool hasFrame() const { return m_frame.isValid(); }

    /// Lazy CPU QImage representation (REQ-SW-PL-032 AC 1/2/3).
    ///
    /// Converts the wrapped frame to a QImage at most once per frame and
    /// caches the result, so N consumers sharing the same VideoFrameData
    /// share a single conversion ("at most one copy per frame representation,
    /// lazy and shared"). The cache is invalidated by setFrame().
    ///
    /// GUI-thread only: the whole node processing graph runs on the GUI
    /// thread, so no mutex is needed around the mutable cache.
    const QImage &asImage() const
    {
        if (m_imageCache.isNull() && m_frame.isValid()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            m_imageCache = m_frame.toImage();
#else
            m_imageCache = m_frame.image();
#endif
        }
        return m_imageCache;
    }

private:
    QVideoFrame m_frame;
    /// Lazy QImage cache — populated on first asImage() call, cleared on
    /// setFrame(). Mutable because asImage() is const (cache is a
    /// representation detail, not part of the frame identity).
    mutable QImage m_imageCache;
};
