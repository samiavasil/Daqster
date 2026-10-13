#include "StreamSourceWidget.h"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

StreamSourceWidget::StreamSourceWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(tr("Stream URL (http:// or rtsp://)"));
    layout->addWidget(m_urlEdit);

    auto* controlRow = new QHBoxLayout();
    m_connectButton = new QPushButton(tr("Connect"), this);
    m_statusLabel = new QLabel(tr("Disconnected"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    controlRow->addWidget(m_connectButton);
    controlRow->addWidget(m_statusLabel, 1);
    layout->addLayout(controlRow);

    connect(m_connectButton, &QPushButton::clicked,
            this, &StreamSourceWidget::connectClicked);
    connect(m_urlEdit, &QLineEdit::textChanged, this, &StreamSourceWidget::urlChanged);
}

void StreamSourceWidget::setUrl(const QString& url)
{
    if (m_urlEdit->text() == url)
        return;
    const QSignalBlocker blocker(m_urlEdit);
    m_urlEdit->setText(url);
}

void StreamSourceWidget::setStatus(const QString& text, bool ok)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok ? QStringLiteral("color: green;")
                                    : QStringLiteral("color: gray;"));
}

void StreamSourceWidget::setPlaying(bool playing)
{
    m_connectButton->setText(playing ? tr("Stop") : tr("Connect"));
}
