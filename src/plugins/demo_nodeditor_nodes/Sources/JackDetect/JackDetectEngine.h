#ifndef JACKDETECTENGINE_H
#define JACKDETECTENGINE_H

#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

/**
 * @brief Polls Linux HDA jack state from /proc/asound on a QTimer.
 *
 * REQ-SW-PL-046: scans the HDA jack files under /proc/asound (the "jack*"
 * files inside each "card*" / "codec#*" directory pair), parses the jack
 * names and present states ("Pin 0x21 (Headphone): present = No") and emits
 * jacksChanged() only when the state actually changes (event-driven).
 *
 * The engine owns a QTimer; start() performs an initial read + emit and begins
 * polling, stop() stops the timer. The caller (JackDetectModel) owns the
 * engine and is responsible for calling stop() on destruction.
 */
class JackDetectEngine : public QObject
{
    Q_OBJECT

public:
    /// One jack's state: name (e.g. "Headphone", "Internal Mic") + plugged flag.
    struct JackState {
        QString name;   // e.g. "Headphone", "Internal Mic"
        bool present = false; // true = plugged

        bool operator==(const JackState &other) const
        {
            return name == other.name && present == other.present;
        }
    };

    explicit JackDetectEngine(QObject *parent = nullptr);
    ~JackDetectEngine() override;

    /// Set the polling interval in ms (clamped to 100..5000, default 500).
    void setPollIntervalMs(int ms);

    /// Start polling: initial read + emit, then start the timer.
    void start();

    /// Stop polling (timer stop — no crash on destruction).
    void stop();

signals:
    void jacksChanged(const QVector<JackDetectEngine::JackState> &jacks);
    void statusChanged(const QString &status);

private:
    void poll();
    QVector<JackState> readJacks() const;

    QTimer *m_timer;
    int m_pollIntervalMs = 500;
    QVector<JackState> m_lastJacks;
};

#endif // JACKDETECTENGINE_H