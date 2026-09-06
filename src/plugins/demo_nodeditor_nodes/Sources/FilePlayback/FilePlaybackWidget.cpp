#include "FilePlaybackWidget.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

FilePlaybackWidget::FilePlaybackWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *pathRow = new QHBoxLayout();
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(4);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Path to .sdf file"));
    pathRow->addWidget(m_pathEdit, 1);

    auto *browseButton = new QPushButton(tr("Browse"), this);
    pathRow->addWidget(browseButton);

    layout->addLayout(pathRow);

    m_playStopButton = new QPushButton(tr("Play"), this);
    layout->addWidget(m_playStopButton);

    m_statusLabel = new QLabel(tr("Idle"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_playStopButton, &QPushButton::clicked,
            this, &FilePlaybackWidget::onPlayStopClicked);
    connect(browseButton, &QPushButton::clicked,
            this, &FilePlaybackWidget::onBrowseClicked);
    connect(m_pathEdit, &QLineEdit::textChanged,
            this, &FilePlaybackWidget::pathChanged);
}

QString FilePlaybackWidget::filePath() const
{
    return m_pathEdit->text().trimmed();
}

void FilePlaybackWidget::setFilePath(const QString &path)
{
    m_pathEdit->setText(path);
}

void FilePlaybackWidget::setStatus(const QString &status)
{
    m_statusLabel->setText(status);
}

void FilePlaybackWidget::onPlayStopClicked()
{
    if (m_playing) {
        m_playing = false;
        m_playStopButton->setText(tr("Play"));
        emit stopRequested();
    } else {
        m_playing = true;
        m_playStopButton->setText(tr("Stop"));
        emit playRequested();
    }
}

void FilePlaybackWidget::onBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Sampled Data"), QString(),
        tr("Sampled Data (*.sdf);;All Files (*)"));
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}
