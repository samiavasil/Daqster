#include "ImageDisplayWidget.h"

#include <QtCore/QStringList>
#include <QtGui/QPixmap>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

ImageDisplayWidget::ImageDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_image = new QLabel(this);
    m_image->setAlignment(Qt::AlignCenter);
    // The preview is already bounded by ImageDisplayNode::kPreviewWidth/Height,
    // so this only keeps the node body from growing with the pixmap.
    m_image->setFixedSize(ImageDisplayNode::kPreviewWidth, ImageDisplayNode::kPreviewHeight);
    m_image->setFrameShape(QFrame::StyledPanel);
    m_image->setText(QStringLiteral("no image"));
    layout->addWidget(m_image);

    m_info = new QLabel(this);
    m_info->setWordWrap(true);
    layout->addWidget(m_info);
}

void ImageDisplayWidget::setSnapshot(ImageDisplaySnapshot const &snapshot)
{
    if (snapshot.thumbnail.isNull()) {
        m_image->setPixmap(QPixmap());
        m_image->setText(QStringLiteral("no image"));
    } else {
        // The pixmap is built from the already-downscaled copy, so this is a
        // bounded allocation — no full-resolution conversion on the GUI thread.
        m_image->setPixmap(QPixmap::fromImage(snapshot.thumbnail));
        m_image->setText(QString());
    }

    const QString age = snapshot.msSinceLast < 0
                            ? QStringLiteral("—")
                            : QStringLiteral("%1 ms ago").arg(snapshot.msSinceLast);
    const QString scale = snapshot.downscaled
                              ? QStringLiteral(" · scaled to %1×%2")
                                    .arg(snapshot.thumbnail.width())
                                    .arg(snapshot.thumbnail.height())
                              : QString();
    m_info->setText(QStringLiteral("%1×%2 · %3%4 · %5")
                        .arg(snapshot.width)
                        .arg(snapshot.height)
                        .arg(snapshot.format)
                        .arg(scale)
                        .arg(age));

    QStringList detail;
    detail << QStringLiteral("source: %1×%2 · %3").arg(snapshot.width).arg(snapshot.height).arg(snapshot.format);
    if (!snapshot.mimeType.isEmpty())
        detail << QStringLiteral("mime: %1").arg(snapshot.mimeType);
    if (!snapshot.model.isEmpty())
        detail << QStringLiteral("model: %1").arg(snapshot.model);
    if (snapshot.metaElapsedMs > 0)
        detail << QStringLiteral("produced in %1 ms").arg(snapshot.metaElapsedMs);
    detail << QStringLiteral("%1 images received").arg(snapshot.messages);
    m_image->setToolTip(detail.join(QLatin1Char('\n')));
    m_info->setToolTip(m_image->toolTip());
}
