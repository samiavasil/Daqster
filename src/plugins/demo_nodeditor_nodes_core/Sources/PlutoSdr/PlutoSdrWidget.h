#ifndef PLUTOSDRWIDGET_H
#define PLUTOSDRWIDGET_H

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

/**
 * @brief Config UI for the PlutoSDR RX DAQ node (REQ-SW-PL-040).
 *
 * URI, frequency (MHz), sample rate (MSPS), gain mode (manual/fast_attack/
 * slow_attack) + manual gain, Start/Stop toggle and a status label
 * (connected/streaming/error). Emits startRequested/stopRequested to the
 * model and configChanged whenever a config control changes (the model
 * forwards it to the engine, applied on next start).
 */
class PlutoSdrWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlutoSdrWidget(QWidget *parent = nullptr);

    // ── Config accessors (used by the model for save/load) ────────────────
    QString uri() const;
    double frequencyMhz() const;
    double sampleRateMsps() const;
    QString gainMode() const;
    double gainDb() const;

    void setUri(const QString &uri);
    void setFrequencyMhz(double mhz);
    void setSampleRateMsps(double msps);
    void setGainMode(const QString &mode);
    void setGainDb(double db);

    bool isStarted() const { return m_started; }

signals:
    void startRequested();
    void stopRequested();
    void configChanged();

public slots:
    void setStatus(const QString &status);

private slots:
    void onStartStopClicked();

private:
    QLineEdit *m_uriEdit = nullptr;
    QDoubleSpinBox *m_freqSpin = nullptr;
    QDoubleSpinBox *m_rateSpin = nullptr;
    QComboBox *m_gainModeCombo = nullptr;
    QDoubleSpinBox *m_gainSpin = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_started = false;
};

#endif // PLUTOSDRWIDGET_H