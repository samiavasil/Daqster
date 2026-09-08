#include "PlutoSdrEngine.h"

#include "LogCategories.h"

#include <QThread>

#include <cerrno>
#include <cstring>

#ifdef HAVE_LIBIIO
#include <iio.h>
#endif

PlutoSdrEngine::PlutoSdrEngine(QObject *parent)
    : QObject(parent)
{
}

PlutoSdrEngine::~PlutoSdrEngine()
{
    close();
}

// ── Config setters (plain members, no libiio) ───────────────────────────────

void PlutoSdrEngine::setUri(const QString &uri)
{
    m_uri = uri;
}

void PlutoSdrEngine::setFrequencyMhz(double mhz)
{
    m_frequencyMhz = mhz;
}

void PlutoSdrEngine::setSampleRateMsps(double msps)
{
    m_sampleRateMsps = msps;
}

void PlutoSdrEngine::setGainMode(const QString &mode)
{
    m_gainMode = mode;
}

void PlutoSdrEngine::setGainDb(double db)
{
    m_gainDb = db;
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

bool PlutoSdrEngine::open()
{
#ifdef HAVE_LIBIIO
    if (m_context)
        return true;

    m_context = iio_create_context_from_uri(m_uri.toUtf8().constData());
    if (!m_context) {
        const QString msg = QStringLiteral("Failed to create libiio context for %1").arg(m_uri);
        qCWarning(lcDemoNodes) << "PlutoSdrEngine:" << msg;
        emit errorOccurred(msg);
        emit statusChanged(QStringLiteral("error: context"));
        return false;
    }

    m_phy = iio_context_find_device(m_context, "ad9361-phy");
    m_rx = iio_context_find_device(m_context, "cf-ad9361-lpc");
    if (!m_phy || !m_rx) {
        const QString msg = QStringLiteral("PlutoSDR devices not found (ad9361-phy / cf-ad9361-lpc)");
        qCWarning(lcDemoNodes) << "PlutoSdrEngine:" << msg;
        emit errorOccurred(msg);
        iio_context_destroy(m_context);
        m_context = nullptr;
        return false;
    }

    m_rxI = iio_device_find_channel(m_rx, "voltage0", false);
    m_rxQ = iio_device_find_channel(m_rx, "voltage1", false);
    if (!m_rxI || !m_rxQ) {
        const QString msg = QStringLiteral("RX channels voltage0/voltage1 not found on cf-ad9361-lpc");
        qCWarning(lcDemoNodes) << "PlutoSdrEngine:" << msg;
        emit errorOccurred(msg);
        iio_context_destroy(m_context);
        m_context = nullptr;
        m_phy = nullptr;
        m_rx = nullptr;
        return false;
    }

    applyConfig();

    emit statusChanged(QStringLiteral("connected"));
    return true;
#else
    const QString msg = QStringLiteral("PlutoSDR node built without libiio support");
    qCWarning(lcDemoNodes) << "PlutoSdrEngine:" << msg;
    emit errorOccurred(msg);
    emit statusChanged(QStringLiteral("error: no libiio"));
    return false;
#endif
}

void PlutoSdrEngine::close()
{
    stop();

#ifdef HAVE_LIBIIO
    if (m_context) {
        iio_context_destroy(m_context);
        m_context = nullptr;
    }
    m_phy = nullptr;
    m_rx = nullptr;
    m_rxI = nullptr;
    m_rxQ = nullptr;
#endif

    emit statusChanged(QStringLiteral("closed"));
}

void PlutoSdrEngine::start()
{
#ifdef HAVE_LIBIIO
    if (m_running.load())
        return;

    if (!m_context && !open())
        return;

    // Re-apply config in case the UI changed since open().
    applyConfig();

    // Enable RX channels (I + Q) before creating the buffer.
    iio_channel_enable(m_rxI);
    iio_channel_enable(m_rxQ);

    m_buffer = iio_device_create_buffer(m_rx, 4096, false);
    if (!m_buffer) {
        const QString msg = QStringLiteral("iio_device_create_buffer failed: %1")
                                .arg(QString::fromLocal8Bit(strerror(errno)));
        qCWarning(lcDemoNodes) << "PlutoSdrEngine:" << msg;
        iio_channel_disable(m_rxI);
        iio_channel_disable(m_rxQ);
        emit errorOccurred(msg);
        emit statusChanged(QStringLiteral("error: buffer"));
        return;
    }

    m_running.store(true);
    m_activeSampleRateHz = m_sampleRateMsps * 1e6;
    m_worker = QThread::create([this] { workerLoop(); });
    m_worker->setObjectName(QStringLiteral("PlutoSdrStreamThread"));
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    m_worker->start();
#else
    const QString msg = QStringLiteral("PlutoSDR node built without libiio support");
    qCWarning(lcDemoNodes) << "PlutoSdrEngine:" << msg;
    emit errorOccurred(msg);
#endif
}

void PlutoSdrEngine::stop()
{
    if (!m_running.load() && !m_worker)
        return;

    m_running.store(false);

#ifdef HAVE_LIBIIO
    // Unblock a pending iio_buffer_refill() (libiio >= 0.24). The worker loop
    // sees the stop flag and exits; the buffer is destroyed after the join.
    if (m_buffer)
        iio_buffer_cancel(m_buffer);
#endif

    if (m_worker) {
        m_worker->wait();
        m_worker = nullptr;
    }

#ifdef HAVE_LIBIIO
    if (m_buffer) {
        iio_buffer_destroy(m_buffer);
        m_buffer = nullptr;
    }
    if (m_rxI)
        iio_channel_disable(m_rxI);
    if (m_rxQ)
        iio_channel_disable(m_rxQ);
#endif

    emit statusChanged(QStringLiteral("stopped"));
}

// ── Worker loop (runs on the stream thread) ─────────────────────────────────

void PlutoSdrEngine::workerLoop()
{
#ifdef HAVE_LIBIIO
    emit statusChanged(QStringLiteral("streaming"));

    while (m_running.load()) {
        const ssize_t n = iio_buffer_refill(m_buffer);
        if (n < 0) {
            if (m_running.load()) {
                const QString msg = QStringLiteral("iio_buffer_refill failed: %1")
                                        .arg(QString::fromLocal8Bit(strerror(static_cast<int>(-n))));
                qCWarning(lcDemoNodes) << "PlutoSdrEngine:" << msg;
                emit errorOccurred(msg);
            }
            m_running.store(false);
            break;
        }
        if (n == 0)
            continue;

        // The whole buffer is the interleaved I/Q stream (voltage0 + voltage1
        // enabled): copy the refilled bytes as-is (int16 interleaved).
        QByteArray buffer(static_cast<const char *>(iio_buffer_start(m_buffer)),
                          static_cast<int>(n));
        emit samplesReady(buffer, m_activeSampleRateHz, 2);
    }

    emit statusChanged(QStringLiteral("stopped"));
#endif
}

// ── RF configuration on ad9361-phy ──────────────────────────────────────────

void PlutoSdrEngine::applyConfig()
{
#ifdef HAVE_LIBIIO
    if (!m_phy)
        return;

    // RX LO frequency (Hz)
    const long long freqHz = static_cast<long long>(m_frequencyMhz * 1e6);
    if (iio_device_attr_write_longlong(m_phy, "out_altvoltage0_RX_LO_frequency", freqHz) < 0)
        qCWarning(lcDemoNodes) << "PlutoSdrEngine: failed to set RX LO frequency";

    // Sample rate (Hz)
    const long long rateHz = static_cast<long long>(m_sampleRateMsps * 1e6);
    if (iio_device_attr_write_longlong(m_phy, "in_voltage_sampling_frequency", rateHz) < 0)
        qCWarning(lcDemoNodes) << "PlutoSdrEngine: failed to set sampling frequency";

    // RF bandwidth — ~80% of the sample rate (standard SDR practice).
    const long long bwHz = static_cast<long long>(m_sampleRateMsps * 0.8e6);
    if (iio_device_attr_write_longlong(m_phy, "in_voltage_rf_bandwidth", bwHz) < 0)
        qCWarning(lcDemoNodes) << "PlutoSdrEngine: failed to set RF bandwidth";

    // Gain control mode (manual / fast_attack / slow_attack)
    if (iio_device_attr_write(m_phy, "in_voltage0_gain_control_mode",
                              m_gainMode.toUtf8().constData()) < 0)
        qCWarning(lcDemoNodes) << "PlutoSdrEngine: failed to set gain control mode";

    // Manual gain (dB) — only meaningful in manual mode.
    if (m_gainMode == QLatin1String("manual")) {
        const QByteArray gainStr = QByteArray::number(m_gainDb, 'f', 6);
        if (iio_device_attr_write(m_phy, "in_voltage0_hardwaregain", gainStr.constData()) < 0)
            qCWarning(lcDemoNodes) << "PlutoSdrEngine: failed to set hardware gain";
    }
#endif
}