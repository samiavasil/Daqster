#include "FileRecordWidget.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

FileRecordWidget::FileRecordWidget(QWidget *parent)
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

    m_startStopButton = new QPushButton(tr("Start"), this);
    layout->addWidget(m_startStopButton);

    m_statusLabel = new QLabel(tr("Idle"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_startStopButton, &QPushButton::clicked,
            this, &FileRecordWidget::onStartStopClicked);
    connect(browseButton, &QPushButton::clicked,
            this, &FileRecordWidget::onBrowseClicked);
    connect(m_pathEdit, &QLineEdit::textChanged,
            this, &FileRecordWidget::pathChanged);
}

QString FileRecordWidget::filePath() const
{
    return m_pathEdit->text().trimmed();
}

void FileRecordWidget::setFilePath(const QString &path)
{
    m_pathEdit->setText(path);
}

void FileRecordWidget::setStatus(const QString &status)
{
    m_statusLabel->setText(status);
}

void FileRecordWidget::onStartStopClicked()
{
    if (m_recording) {
        m_recording = false;
        m_startStopButton->setText(tr("Start"));
        emit stopRequested();
    } else {
        m_recording = true;
        m_startStopButton->setText(tr("Stop"));
        emit startRequested();
    }
}

void FileRecordWidget::onBrowseClicked()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Sampled Data"), QString(),
        tr("Sampled Data (*.sdf);;All Files (*)"));
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}
