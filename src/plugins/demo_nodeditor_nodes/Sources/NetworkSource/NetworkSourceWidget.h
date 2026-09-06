#ifndef NETWORKSOURCEWIDGET_H
#define NETWORKSOURCEWIDGET_H

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

/**
 * @brief Config UI for the Network Source node (REQ-SW-PL-044).
 *
 * Protocol (UDP/TCP), listen port, sampleRate (Hz), channel count + channel
 * type (INT16/FLOAT32), a Start/Stop toggle and a status label showing the
 * bytes received. Emits startRequested/stopRequested to the model.
 */
class NetworkSourceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkSourceWidget(QWidget *parent = nullptr);

    QString protocol() const;
    void setProtocol(const QString &protocol);

    int port() const;
    void setPort(int port);

    double sampleRate() const;
    void setSampleRate(double rate);

    int channelCount() const;
    void setChannelCount(int count);

    QString channelType() const;
    void setChannelType(const QString &type);

    bool isRunning() const { return m_running; }

signals:
    void startRequested();
    void stopRequested();

public slots:
    void setStatus(const QString &status);

private slots:
    void onStartStopClicked();

private:
    QComboBox *m_protocolCombo = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QDoubleSpinBox *m_sampleRateSpin = nullptr;
    QSpinBox *m_channelCountSpin = nullptr;
    QComboBox *m_channelTypeCombo = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_running = false;
};

#endif // NETWORKSOURCEWIDGET_H
