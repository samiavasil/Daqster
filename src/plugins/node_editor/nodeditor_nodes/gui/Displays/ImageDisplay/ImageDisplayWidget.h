#pragma once

#include <QtWidgets/QWidget>

#include "Displays/ImageDisplay/ImageDisplayNode.h"

class QLabel;

/**
 * @brief Body of the Image Display node (REQ-SW-PL-090).
 *
 * Shows the downscaled preview the model's worker pass produced plus the
 * source geometry/format and the age of the block. The full-resolution image
 * never reaches this widget — that is the point of the core-side downscale.
 */
class ImageDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageDisplayWidget(QWidget *parent = nullptr);
    ~ImageDisplayWidget() override = default;

    QLabel *imageLabel() const { return m_image; }
    QLabel *infoLabel() const { return m_info; }

public slots:
    /// Rendered by ImageDisplayNode::snapshotReady().
    void setSnapshot(ImageDisplaySnapshot const &snapshot);

private:
    QLabel *m_image = nullptr;
    QLabel *m_info = nullptr;
};
