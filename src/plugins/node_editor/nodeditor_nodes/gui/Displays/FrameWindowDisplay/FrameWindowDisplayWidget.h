#pragma once

#include <QtWidgets/QWidget>

#include <array>

#include "Displays/FrameWindowDisplay/FrameWindowDisplayNode.h"

class QLabel;

/**
 * @brief Body of the Frame Window Display node (REQ-SW-PL-090).
 *
 * A 2×2 grid of thumbnails plus the window metadata (frame count, first/last
 * arrival time, span). When no thumbnail can be produced (GPU-resident frames,
 * unsupported pixel formats) the widget says so explicitly instead of showing
 * four empty boxes — the metadata is still the truth about the window.
 */
class FrameWindowDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FrameWindowDisplayWidget(QWidget *parent = nullptr);
    ~FrameWindowDisplayWidget() override = default;

    /// Thumbnail label by index, or nullptr when out of range.
    QLabel *thumbnailLabel(int index) const
    {
        if (index < 0 || index >= FrameWindowDisplayNode::kMaxThumbnails)
            return nullptr;
        return m_thumbs[static_cast<std::size_t>(index)];
    }
    QLabel *infoLabel() const { return m_info; }
    QLabel *noteLabel() const { return m_note; }

public slots:
    /// Rendered by FrameWindowDisplayNode::snapshotReady().
    void setSnapshot(FrameWindowDisplaySnapshot const &snapshot);

private:
    std::array<QLabel *, FrameWindowDisplayNode::kMaxThumbnails> m_thumbs{{nullptr, nullptr,
                                                                           nullptr, nullptr}};
    QLabel *m_info = nullptr;
    QLabel *m_note = nullptr;
};
