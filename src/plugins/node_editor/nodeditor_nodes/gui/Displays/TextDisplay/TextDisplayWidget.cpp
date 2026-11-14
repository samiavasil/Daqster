#include "TextDisplayWidget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

namespace {

/// Tooltips are GUI strings, not transport: a 1 MB answer must not become a
/// 1 MB tooltip, so the detail text is clipped here (the counters above it
/// still report the FULL length).
constexpr int kMaxTooltipChars = 4000;

/// Node-body preview height in text lines — keeps the node compact.
constexpr int kPreviewLines = 6;

} // namespace

TextDisplayWidget::TextDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_preview = new QLabel(this);
    m_preview->setWordWrap(true);
    m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_preview->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    // Six lines is enough to see what the stream carries; the tooltip has the
    // whole text.
    m_preview->setMinimumHeight(m_preview->fontMetrics().lineSpacing() * kPreviewLines);
    m_preview->setMaximumHeight(m_preview->fontMetrics().lineSpacing() * kPreviewLines);
    m_preview->setFrameShape(QFrame::StyledPanel);
    layout->addWidget(m_preview);

    m_stats = new QLabel(this);
    m_stats->setWordWrap(true);
    layout->addWidget(m_stats);
}

void TextDisplayWidget::setSnapshot(TextDisplaySnapshot const &snapshot)
{
    m_preview->setText(snapshot.preview);

    const QString age = snapshot.msSinceLast < 0
                            ? QStringLiteral("—")
                            : QStringLiteral("%1 ms ago").arg(snapshot.msSinceLast);
    m_stats->setText(QStringLiteral("%1 blocks · %2 chars · %3 lines · %4")
                         .arg(snapshot.messages)
                         .arg(snapshot.characters)
                         .arg(snapshot.lines)
                         .arg(age));

    QString detail = snapshot.full;
    if (detail.size() > kMaxTooltipChars)
        detail = detail.left(kMaxTooltipChars) + QStringLiteral("\n… (tooltip clipped)");
    m_preview->setToolTip(detail);
    // The QLabel would otherwise report the preview as its tooltip target.
    m_stats->setToolTip(detail);

    // Do not let a long preview force the node to grow — the scene asks for the
    // widget's size hint on every layout pass.
    m_preview->adjustSize();
}
