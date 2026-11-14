#include "Dsp/FirDecimator.h"

#include <algorithm>

namespace {

constexpr int kCompactThreshold = 8192;

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

} // namespace

bool FirDecimator::configure(int decimation, const FirCoefficients &coefficients,
                             QString *error)
{
    setError(error, QString());
    if (decimation < 1) {
        setError(error, QStringLiteral("decimation must be >= 1, got %1").arg(decimation));
        return false;
    }
    if (!coefficients.validate(error))
        return false;

    m_decimation = decimation;
    m_taps = coefficients.tapCount();
    m_coefficients = coefficients;
    reset();
    return true;
}

void FirDecimator::reset()
{
    m_buffer.clear();
    m_bufferStart = 0;
    m_readPos = 0;
    m_received = 0;
    m_produced = 0;
}

double FirDecimator::latencySamples() const
{
    if (!isConfigured())
        return 0.0;
    return double(m_taps - 1) / (2.0 * double(m_decimation));
}

double FirDecimator::latencyInputSamples() const
{
    if (!isConfigured())
        return 0.0;
    return double(m_taps - 1) / 2.0;
}

void FirDecimator::process(const float *in, int inFrames, QVector<float> &out)
{
    out.clear();
    if (!isConfigured())
        return;

    if (in && inFrames > 0) {
        m_buffer.reserve(m_buffer.size() + inFrames);
        for (int i = 0; i < inFrames; ++i)
            m_buffer.append(in[i]);
        m_received += inFrames;
    }

    const QVector<double> &taps = m_coefficients.taps();
    const qint64 lastAvailable = m_received - 1;

    while (m_produced * qint64(m_decimation) <= lastAvailable) {
        const qint64 q = m_produced * qint64(m_decimation);
        double accumulator = 0.0;
        // y[q/M] = Σ h[k]·x[q − k]; indices before the stream start are 0.
        for (int k = 0; k < m_taps; ++k) {
            const qint64 index = q - k;
            if (index < 0)
                continue;
            if (index < m_bufferStart)
                continue;  // only happens past the stream head, i.e. never
            const int offset = m_readPos + int(index - m_bufferStart);
            accumulator += taps.at(k) * double(m_buffer.at(offset));
        }
        out.append(float(accumulator));
        ++m_produced;
    }

    // Drop what no future output can reach: the next output at q' needs
    // q' − (taps − 1) … q'.
    const qint64 dropBefore = m_produced * qint64(m_decimation) - qint64(m_taps - 1);
    const qint64 availableEnd = m_bufferStart + (m_buffer.size() - m_readPos);
    const qint64 newStart = std::min(std::max(dropBefore, m_bufferStart), availableEnd);
    m_readPos += int(newStart - m_bufferStart);
    m_bufferStart = newStart;

    if (m_readPos > kCompactThreshold) {
        m_buffer.remove(0, m_readPos);
        m_readPos = 0;
    }
}
