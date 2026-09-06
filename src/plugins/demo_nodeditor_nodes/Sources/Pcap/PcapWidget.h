#ifndef PCAPWIDGET_H
#define PCAPWIDGET_H

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QSpinBox;

/**
 * @brief Config UI for the pcap Packet Capture source node (REQ-SW-PL-047).
 *
 * Provides an interface selector (dropdown from pcap_findalldevs()), a BPF
 * filter text field, snaplen spin box, promiscuous checkbox, Start/Stop
 * buttons, and a status label showing packets captured/dropped/errors.
 * Emits signals that the owning PcapModel connects to the engine.
 */
class PcapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PcapWidget(QWidget *parent = nullptr);

    /// Populate the interface dropdown with available devices.
    void setInterfaces(const QStringList &interfaces);

    /// Current interface name.
    QString interface() const;

    /// Current BPF filter expression.
    QString filter() const;

    /// Current snaplen value.
    int snaplen() const;

    /// Current promiscuous mode setting.
    bool promiscuous() const;

    /// Update the status label with capture statistics.
    void updateStats(quint64 captured, quint64 dropped, quint64 ifDropped);

    /// Update the status label with a plain status/error string.
    void setStatusText(const QString &text);

    /// Reflect the running state in the Start/Stop button.
    void setRunning(bool running);

signals:
    void startRequested();
    void stopRequested();
    void interfaceChanged(const QString &interface);
    void filterChanged(const QString &filter);
    void snaplenChanged(int snaplen);
    void promiscuousChanged(bool promiscuous);

private:
    QComboBox *m_interfaceCombo = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QSpinBox *m_snaplenSpin = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // PCAPWIDGET_H