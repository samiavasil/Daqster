#ifndef PLUTOSDRENGINE_H
#define PLUTOSDRENGINE_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>

#include <atomic>

#ifdef HAVE_LIBIIO
struct iio_context;
struct iio_device;
struct iio_channel;
struct iio_buffer;
#endif

/**
 * @brief libiio wrapper for the PlutoSDR RX DAQ node (REQ-SW-PL-040).
 *
 * Owns the libiio context (URI → ad9361-phy + cf-ad9361-lpc), applies the
 * RF configuration (frequency / sample rate / gain) and streams IQ samples
 * (int16 interleaved) in a dedicated worker thread. The worker loop calls
 * iio_buffer_refill() until the atomic stop flag is set; stop() unblocks a
 * pending refill with iio_buffer_cancel() (libiio >= 0.24) and joins the
 * thread — no deadlock, no crash on graph/app teardown while streaming.
 *
 * The engine is deliberately Qt-free of libiio: all libiio calls are guarded
 * by HAVE_LIBIIO so the file compiles (as empty stubs) on machines without
 * libiio — the node is simply not registered there (CMake auto-detect).
 *
 * Threading: samplesReady/statusChanged/errorOccurred are emitted from the
 * worker thread; the model connects with auto (queued) connections.
 */
class PlutoSdrEngine : public QObject
{
    Q_OBJECT

public:
    explicit PlutoSdrEngine(QObject *parent = nullptr);
    ~PlutoSdrEngine() override;

    // ── Config (applied on next start) ────────────────────────────────────
    void setUri(const QString &uri);          // default "ip:192.168.2.1"
    void setFrequencyMhz(double mhz);         // 70-6000
    void setSampleRateMsps(double msps);      // 0.2-7.5 (USB practical limit)
    void setGainMode(const QString &mode);    // "manual" | "fast_attack" | "slow_attack"
    void setGainDb(double db);                // manual gain

    // ── Lifecycle ─────────────────────────────────────────────────────────
    bool open();                              // create context + find devices
    void close();
    void start();                             // enable channels + buffer + worker thread
    void stop();                              // atomic flag + iio_buffer_cancel + join

    bool isStreaming() const { return m_running.load(); }

signals:
    void samplesReady(const QByteArray &buffer, double sampleRateHz, int channels);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &message);

private:
    void workerLoop();
    void applyConfig();

    QString m_uri = QStringLiteral("ip:192.168.2.1");
    double m_frequencyMhz = 98.5;
    double m_sampleRateMsps = 2.4;
    QString m_gainMode = QStringLiteral("manual");
    double m_gainDb = 30.0;

    std::atomic<bool> m_running{false};
    QThread *m_worker = nullptr;
    double m_activeSampleRateHz = 0.0; // rate actually applied at start()

#ifdef HAVE_LIBIIO
    struct iio_context *m_context = nullptr;
    struct iio_device *m_phy = nullptr;
    struct iio_device *m_rx = nullptr;
    struct iio_channel *m_rxI = nullptr;
    struct iio_channel *m_rxQ = nullptr;
    struct iio_buffer *m_buffer = nullptr;
#endif
};

#endif // PLUTOSDRENGINE_H