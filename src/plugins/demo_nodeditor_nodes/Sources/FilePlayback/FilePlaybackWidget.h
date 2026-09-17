#ifndef FILEPLAYBACKWIDGET_H
#define FILEPLAYBACKWIDGET_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

/**
 * @brief Config UI for the File Playback source node (REQ-SW-PL-043).
 *
 * File path (with Browse via QFileDialog), a Play/Stop toggle and a status
 * label showing the playback position/duration (e.g. "1.2s / 10.0s"). Emits
 * playRequested/stopRequested to the model and pathChanged whenever the path
 * field changes.
 */
class FilePlaybackWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FilePlaybackWidget(QWidget *parent = nullptr);

    QString filePath() const;
    void setFilePath(const QString &path);

    bool isPlaying() const { return m_playing; }

signals:
    void playRequested();
    void stopRequested();
    void pathChanged(const QString &path);

public slots:
    void setStatus(const QString &status);

private slots:
    void onPlayStopClicked();
    void onBrowseClicked();

private:
    QLineEdit *m_pathEdit = nullptr;
    QPushButton *m_playStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_playing = false;
};

#endif // FILEPLAYBACKWIDGET_H
