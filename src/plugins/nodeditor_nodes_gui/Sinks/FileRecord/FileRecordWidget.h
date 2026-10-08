#ifndef FILERECORDWIDGET_H
#define FILERECORDWIDGET_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

/**
 * @brief Config UI for the File Record sink node (REQ-SW-PL-043).
 *
 * File path (with Browse via QFileDialog), a Start/Stop recording toggle and a
 * status label showing the recording state + bytes written. Emits
 * startRequested/stopRequested to the model and pathChanged whenever the path
 * field changes.
 */
class FileRecordWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileRecordWidget(QWidget *parent = nullptr);

    QString filePath() const;
    void setFilePath(const QString &path);

    bool isRecording() const { return m_recording; }

signals:
    void startRequested();
    void stopRequested();
    void pathChanged(const QString &path);

public slots:
    void setStatus(const QString &status);

private slots:
    void onStartStopClicked();
    void onBrowseClicked();

private:
    QLineEdit *m_pathEdit = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_recording = false;
};

#endif // FILERECORDWIDGET_H
