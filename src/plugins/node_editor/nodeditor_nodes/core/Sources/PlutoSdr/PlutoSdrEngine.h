#ifndef PLUTOSDRENGINE_H
#define PLUTOSDRENGINE_H

#include "nodeditor_nodes_core_export.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <Threading/ThreadOwner.h>

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
class NODEDITOR_NODES_CORE_EXPORT PlutoSdrEngine : public QObject
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

    /// How a stop attempt ended (REQ-SW-PL-050 D3).
    enum class StopResult {
        NotRunning, ///< nothing streaming
        Stopped,    ///< the stream thread was joined
        TimedOut    ///< it did not return within the join deadline (logged)
    };

    /// Atomic flag + iio_buffer_cancel + **bounded** join. Never waits forever:
    /// terminate() is not used because the worker is almost certainly inside
    /// iio_buffer_refill() on a buffer this method would then destroy.
    StopResult stop();

    bool isStreaming() const { return m_running.load(); }

signals:
    void samplesReady(const QByteArray &buffer, double sampleRateHz, int channels);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &message);

public:
    // Accessors for model save/load
    const QString& uri() const { return m_uri; }
    double frequencyMhz() const { return m_frequencyMhz; }
    double sampleRateMsps() const { return m_sampleRateMsps; }
    const QString& gainMode() const { return m_gainMode; }
    double gainDb() const { return m_gainDb; }

private:
    void workerLoop();
    void applyConfig();

    QString m_uri = QStringLiteral("ip:192.168.2.1");
    double m_frequencyMhz = 98.5;
    double m_sampleRateMsps = 2.4;
    QString m_gainMode = QStringLiteral("manual");
    double m_gainDb = 30.0;

    std::atomic<bool> m_running{false};
    /// Owns the stream thread (REQ-SW-PL-050). The worker is a plain QObject
    /// context, not an event-loop worker: the loop is a blocking
    /// `iio_buffer_refill()` and runs as a queued call on the owned thread.
    /// `stop(..., terminateOnTimeout=false)` reports a stuck libiio call instead
    /// of killing the thread; `detach()` then gives the thread up on purpose.
    Daqster::ThreadOwner<QObject> m_stream;
    /// Set when the stream thread could not be joined: the iio context, buffer
    /// and thread are deliberately left alive (destroying them under a live
    /// `iio_buffer_refill()` is undefined), so close() must not free them.
    bool m_streamLeftAttached = false;
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