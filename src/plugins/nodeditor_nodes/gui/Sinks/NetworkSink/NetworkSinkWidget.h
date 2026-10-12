#ifndef NETWORKSINKWIDGET_H
#define NETWORKSINKWIDGET_H

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/**
 * @brief Config UI for the Network Sink node (REQ-SW-PL-044).
 *
 * Protocol (UDP/TCP), destination host + port, a Start/Stop toggle and a
 * status label showing the bytes sent. Emits startRequested/stopRequested to
 * the model.
 */
class NetworkSinkWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkSinkWidget(QWidget *parent = nullptr);

    QString protocol() const;
    void setProtocol(const QString &protocol);

    QString host() const;
    void setHost(const QString &host);

    int port() const;
    void setPort(int port);

    bool isRunning() const { return m_running; }

    QString status() const;

signals:
    void startRequested();
    void stopRequested();

public slots:
    void setStatus(const QString &status);

private slots:
    void onStartStopClicked();

private:
    QComboBox *m_protocolCombo = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_running = false;
};

#endif // NETWORKSINKWIDGET_H
