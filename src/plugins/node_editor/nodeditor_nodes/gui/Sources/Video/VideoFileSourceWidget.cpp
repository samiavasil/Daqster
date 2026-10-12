#include "VideoFileSourceWidget.h"

#include <QtCore/QSignalBlocker>
#include <QtCore/QDir>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

VideoFileSourceWidget::VideoFileSourceWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* fileRow = new QHBoxLayout();
    m_fileEdit = new QLineEdit(this);
    m_fileEdit->setPlaceholderText(tr("Path to video file"));
    auto* browseButton = new QPushButton(tr("..."), this);
    browseButton->setMaximumWidth(32);
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(browseButton);
    layout->addLayout(fileRow);

    auto* controlRow = new QHBoxLayout();
    m_playPauseButton = new QPushButton(tr("Play"), this);
    m_statusLabel = new QLabel(tr("Stopped"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    controlRow->addWidget(m_playPauseButton);
    controlRow->addWidget(m_statusLabel, 1);
    layout->addLayout(controlRow);

    auto* seekRow = new QHBoxLayout();
    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setEnabled(false);
    m_seekBackButton = new QPushButton(tr("<< -5s"), this);
    m_seekBackButton->setEnabled(false);
    m_seekForwardButton = new QPushButton(tr(">> +5s"), this);
    m_seekForwardButton->setEnabled(false);
    m_timeLabel = new QLabel(tr("0:00 / 0:00"), this);
    seekRow->addWidget(m_stopButton);
    seekRow->addWidget(m_seekBackButton);
    seekRow->addWidget(m_seekForwardButton);
    seekRow->addWidget(m_timeLabel, 1);
    layout->addLayout(seekRow);

    connect(browseButton, &QPushButton::clicked, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(
            this,
            tr("Select video file"),
            QString(),
            tr("Video files (*.mp4 *.avi *.mkv *.mov *.webm *.m4v *.mpg *.mpeg);;All files (*)"));
        if (!filePath.isEmpty())
            m_fileEdit->setText(QDir::toNativeSeparators(filePath));
    });
    connect(m_fileEdit, &QLineEdit::textChanged, this, &VideoFileSourceWidget::filePathChanged);
    connect(m_playPauseButton, &QPushButton::clicked, this, &VideoFileSourceWidget::playPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &VideoFileSourceWidget::stopClicked);
    connect(m_seekBackButton, &QPushButton::clicked, this, &VideoFileSourceWidget::seekBackClicked);
    connect(m_seekForwardButton, &QPushButton::clicked, this, &VideoFileSourceWidget::seekForwardClicked);
}

void VideoFileSourceWidget::setFilePath(const QString& filePath)
{
    if (m_fileEdit->text() == filePath)
        return;
    const QSignalBlocker blocker(m_fileEdit);
    m_fileEdit->setText(filePath);
}

void VideoFileSourceWidget::setStatus(const QString& text, bool ok)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok ? QStringLiteral("color: green;")
                                    : QStringLiteral("color: gray;"));
}

void VideoFileSourceWidget::setPlaying(bool playing)
{
    m_playPauseButton->setText(playing ? tr("Pause") : tr("Play"));
}

void VideoFileSourceWidget::setTransportEnabled(bool enabled)
{
    m_stopButton->setEnabled(enabled);
    m_seekBackButton->setEnabled(enabled);
    m_seekForwardButton->setEnabled(enabled);
}

void VideoFileSourceWidget::setPosition(qint64 positionMs, qint64 durationMs)
{
    const qint64 posSecs = positionMs / 1000;
    const qint64 durSecs = durationMs / 1000;
    m_timeLabel->setText(QString("%1:%2 / %3:%4")
                             .arg(posSecs / 60, 2, 10, QChar('0'))
                             .arg(posSecs % 60, 2, 10, QChar('0'))
                             .arg(durSecs / 60, 2, 10, QChar('0'))
                             .arg(durSecs % 60, 2, 10, QChar('0')));
}
