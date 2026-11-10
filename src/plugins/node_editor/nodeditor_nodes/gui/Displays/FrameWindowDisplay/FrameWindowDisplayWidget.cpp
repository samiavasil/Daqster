#include "FrameWindowDisplayWidget.h"

#include <QtGui/QPixmap>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

namespace {
constexpr int kColumns = 2;
} // namespace

FrameWindowDisplayWidget::FrameWindowDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(1);

    for (int i = 0; i < FrameWindowDisplayNode::kMaxThumbnails; ++i) {
        auto *thumb = new QLabel(this);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setFixedSize(FrameWindowDisplayNode::kThumbnailWidth,
                            FrameWindowDisplayNode::kThumbnailWidth * 3 / 4);
        thumb->setFrameShape(QFrame::StyledPanel);
        thumb->setText(QStringLiteral("—"));
        m_thumbs[i] = thumb;
        grid->addWidget(thumb, i / kColumns, i % kColumns);
    }
    layout->addLayout(grid);

    m_info = new QLabel(this);
    m_info->setWordWrap(true);
    layout->addWidget(m_info);

    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    m_note->setVisible(false);
    layout->addWidget(m_note);
}

void FrameWindowDisplayWidget::setSnapshot(FrameWindowDisplaySnapshot const &snapshot)
{
    // Thumbnails: the model sends at most kMaxThumbnails, already downscaled.
    for (int i = 0; i < FrameWindowDisplayNode::kMaxThumbnails; ++i) {
        QLabel *thumb = m_thumbs[i];
        if (i < snapshot.thumbnails.size() && !snapshot.thumbnails.at(i).isNull()) {
            thumb->setPixmap(QPixmap::fromImage(snapshot.thumbnails.at(i)));
            thumb->setText(QString());
        } else {
            thumb->setPixmap(QPixmap());
            thumb->setText(QStringLiteral("—"));
        }
    }

    const QString age = snapshot.msSinceLast < 0
                            ? QStringLiteral("—")
                            : QStringLiteral("%1 ms ago").arg(snapshot.msSinceLast);
    m_info->setText(QStringLiteral("%1 frames · %2…%3 ms (span %4) · %5")
                        .arg(snapshot.count)
                        .arg(snapshot.firstTimestampMs)
                        .arg(snapshot.lastTimestampMs)
                        .arg(snapshot.spanMs)
                        .arg(age));

    // Say it instead of painting empty boxes: a GPU-resident frame cannot be
    // converted off the GUI thread (asImage() is a GL readback), so the window
    // is shown as metadata only.
    if (snapshot.metadataOnly) {
        m_note->setText(snapshot.skipped > 0
                            ? QStringLiteral("metadata only — %1 frame(s) not CPU-convertible")
                                  .arg(snapshot.skipped)
                            : QStringLiteral("metadata only — no frame to preview"));
        m_note->setVisible(true);
    } else if (snapshot.skipped > 0) {
        m_note->setText(QStringLiteral("%1 of %2 frame(s) have no preview")
                            .arg(snapshot.skipped)
                            .arg(snapshot.count));
        m_note->setVisible(true);
    } else {
        m_note->clear();
        m_note->setVisible(false);
    }
}
