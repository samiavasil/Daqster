#include "Dsp/PolyphaseResampler.h"

#include <algorithm>

namespace {

constexpr int kCompactThreshold = 8192;

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

} // namespace

bool PolyphaseResampler::configure(int up, int down, const FirCoefficients &coefficients,
                                   QString *error)
{
    setError(error, QString());
    if (up < 1) {
        setError(error, QStringLiteral("interpolation factor must be >= 1, got %1").arg(up));
        return false;
    }
    if (down < 1) {
        setError(error, QStringLiteral("decimation factor must be >= 1, got %1").arg(down));
        return false;
    }
    if (!coefficients.validate(error))
        return false;

    m_up = up;
    m_down = down;
    m_taps = coefficients.tapCount();
    m_coefficients = coefficients;

    // × up: the prototype was designed for a stream that had zeros inserted
    // into it, so feeding it the real samples needs the missing energy back.
    m_phases.resize(m_taps);
    const QVector<double> &taps = coefficients.taps();
    for (int k = 0; k < m_taps; ++k)
        m_phases[k] = taps.at(k) * double(up);

    reset();
    return true;
}

void PolyphaseResampler::reset()
{
    m_buffer.clear();
    m_bufferStart = 0;
    m_readPos = 0;
    m_received = 0;
    m_produced = 0;
}

double PolyphaseResampler::latencySamples() const
{
    if (!isConfigured())
        return 0.0;
    return double(m_taps - 1) / (2.0 * double(m_down));
}

double PolyphaseResampler::latencyInputSamples() const
{
    if (!isConfigured())
        return 0.0;
    return double(m_taps - 1) / (2.0 * double(m_up));
}

void PolyphaseResampler::process(const float *in, int inFrames, QVector<float> &out)
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

    const int nph = phaseLength();
    const qint64 lastAvailable = m_received - 1;

    while (true) {
        const qint64 scaled = m_produced * qint64(m_down);
        const qint64 q = scaled / qint64(m_up);
        if (q > lastAvailable)
            break;
        const int phase = int(scaled % qint64(m_up));

        double accumulator = 0.0;
        // y[i] = Σ_j h[phase + j·up]·x[q − j]; indices before the head are 0.
        for (int j = 0; j < nph; ++j) {
            const int tapIndex = phase + j * m_up;
            if (tapIndex >= m_taps)
                break;
            const qint64 index = q - j;
            if (index < 0 || index < m_bufferStart)
                continue;
            accumulator += m_phases.at(tapIndex) * double(m_buffer.at(m_readPos + int(index - m_bufferStart)));
        }
        out.append(float(accumulator));
        ++m_produced;
    }

    // The next output needs input indices q' − (nph − 1) … q'.
    const qint64 nextScaled = m_produced * qint64(m_down);
    const qint64 nextQ = nextScaled / qint64(m_up);
    const qint64 dropBefore = nextQ - qint64(nph - 1);
    const qint64 availableEnd = m_bufferStart + (m_buffer.size() - m_readPos);
    const qint64 newStart = std::min(std::max(dropBefore, m_bufferStart), availableEnd);
    m_readPos += int(newStart - m_bufferStart);
    m_bufferStart = newStart;

    if (m_readPos > kCompactThreshold) {
        m_buffer.remove(0, m_readPos);
        m_readPos = 0;
    }
}
