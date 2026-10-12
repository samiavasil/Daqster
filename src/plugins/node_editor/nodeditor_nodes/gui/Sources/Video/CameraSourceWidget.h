#pragma once

#include <QtWidgets/QWidget>

#include <QStringList>

class QComboBox;
class QLabel;
class QPushButton;

/**
 * @brief GUI widget of the Camera Source node (REQ-SW-PL-051 core/gui split).
 *
 * Owns the device selector, the Start/Stop button and the status line. The
 * capture itself (QCamera + frame probe) stays in the core CameraSourceNode,
 * which reports the device list and the status through signals.
 */
class CameraSourceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraSourceWidget(QWidget* parent = nullptr);
    ~CameraSourceWidget() override = default;

public slots:
    /// Model → widget: rebuild the selector and highlight `selectedIndex`.
    void setDevices(const QStringList& descriptions, int selectedIndex);
    /// Model → widget: status line text + color.
    void setStatus(const QString& text, bool ok);
    /// Model → widget: switch the button between Start and Stop.
    void setRunning(bool running);

signals:
    void deviceIndexChanged(int index);
    void startStopRequested();

private:
    QComboBox* m_deviceCombo = nullptr;
    QPushButton* m_startStopButton = nullptr;
    QLabel* m_statusLabel = nullptr;
};
