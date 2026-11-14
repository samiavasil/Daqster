#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QImage>
#include <QString>
#include <QtGlobal>

#include <utility>

/**
 * @brief Single-image node data type (REQ-SW-PL-080).
 *
 * A still image, as opposed to the streaming VideoFrameData. Transported
 * through the graph as std::shared_ptr<NodeData>: only the reference count is
 * bumped, the pixel buffer is never copied. The copy constructor and the copy
 * assignment are deleted for the same reason as in VideoFrameData — a by-value
 * copy would detach the QImage and silently start duplicating the pixels,
 * which is exactly the defect the previous, removed ImageData had (it returned
 * a copy on every read).
 *
 * Access is by const reference (`image()`), so no read ever detaches. Metadata
 * travels next to the pixels and is enough to make the result reproducible
 * (prompt, seed, model, mime type, elapsed time). Width and height are NOT in
 * the metadata: they are properties of the image and are read from it.
 *
 * Dependencies: QImage (QtGui) and QtNodes only — no QtWidgets, no
 * QtMultimedia, so the type is usable in a headless host.
 */
class ImageData : public QtNodes::NodeData
{
public:
    /** Reproducibility metadata. An empty field means "unknown", not an error. */
    struct Meta
    {
        QString prompt;
        QString negativePrompt;
        qint64 seed = -1;
        QString model;
        QString mimeType;
        qint64 elapsedMs = 0;
    };

    ImageData() = default;

    ImageData(QImage image, Meta meta)
        : m_image(std::move(image))
        , m_meta(std::move(meta))
    {}

    ImageData(const ImageData &) = delete;
    ImageData &operator=(const ImageData &) = delete;

    QtNodes::NodeDataType type() const override
    {
        return {"image", "Image"};
    }

    /** The image itself — by const reference, never a copy. */
    const QImage &image() const { return m_image; }

    /** Reproducibility metadata that accompanies the image. */
    const Meta &meta() const { return m_meta; }

    /** True when there is no image at all (QImage::isNull()). */
    bool isEmpty() const { return m_image.isNull(); }

    /** Image width — read from the image, not duplicated in Meta. */
    int width() const { return m_image.width(); }

    /** Image height — read from the image, not duplicated in Meta. */
    int height() const { return m_image.height(); }

private:
    QImage m_image;
    Meta m_meta;
};
