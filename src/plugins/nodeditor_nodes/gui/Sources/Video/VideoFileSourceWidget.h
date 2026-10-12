#pragma once

#include <QtWidgets/QWidget>

#include <QtCore/QtGlobal>

class QLabel;
class QLineEdit;
class QPushButton;

/**
 * @brief GUI widget of the Video File Source node (REQ-SW-PL-051 core/gui split).
 *
 * Owns the path field, the "..." browse button, the transport buttons
 * (Play/Pause, Stop, seek ±5 s), the time label and the status line. The
 * QMediaPlayer and the frame probe stay in the core VideoFileSourceNode; this
 * widget also hosts the file dialog, which cannot live in a headless model.
 */
class VideoFileSourceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoFileSourceWidget(QWidget* parent = nullptr);
    ~VideoFileSourceWidget() override = default;

public slots:
    /// Model → widget: the file path was set programmatically (load()).
    void setFilePath(const QString& filePath);
    /// Model → widget: status line text + color.
    void setStatus(const QString& text, bool ok);
    /// Model → widget: switch the button between Play and Pause.
    void setPlaying(bool playing);
    /// Model → widget: enable/disable Stop and the seek buttons.
    void setTransportEnabled(bool enabled);
    /// Model → widget: the playback head moved.
    void setPosition(qint64 positionMs, qint64 durationMs);

signals:
    void browseClicked();
    void filePathChanged(const QString& filePath);
    void playPauseClicked();
    void stopClicked();
    void seekBackClicked();
    void seekForwardClicked();

private:
    QLineEdit* m_fileEdit = nullptr;
    QPushButton* m_playPauseButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_seekBackButton = nullptr;
    QPushButton* m_seekForwardButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_timeLabel = nullptr;
};
