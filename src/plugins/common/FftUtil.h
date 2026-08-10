#pragma once

// Shared radix-2 FFT utilities (REQ-SW-PL-023).
//
// magnitudeSpectrum() is the canonical magnitude-spectrum implementation for
// the DAQ display path. It mirrors the (frozen) math of
// DaqDisplayNode::spectrumSamples() so results are identical on the same
// input: Hann window, Cooley-Tukey radix-2 DIT (bit reversal + butterflies),
// magnitude normalized by 1/fftSize, half spectrum (0 .. Nyquist).

#include <QVector>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace FftUtil {

namespace detail {

/**
 * @brief Cached Hann window (periodic-denominator variant, matches the DAQ
 * display reference).
 *
 * The window is computed once per FFT size and reused across calls. The cache
 * map and its guard mutex are function-local statics, so initialization is
 * thread-safe on first use and lookups are serialized.
 */
inline QVector<double> hannWindow(int fftSize)
{
    static std::mutex cacheMutex;
    static std::unordered_map<int, QVector<double>> cache;

    std::lock_guard<std::mutex> lock(cacheMutex);
    const auto it = cache.find(fftSize);
    if (it != cache.end())
        return it->second;

    QVector<double> window(fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (fftSize - 1)));
    cache.emplace(fftSize, window);
    return window;
}

} // namespace detail

/**
 * @brief Radix-2 magnitude spectrum with cached Hann window. Thread-safe.
 *
 * Samples of any length are accepted; the transform size is the largest power
 * of two not exceeding both the sample count and maxFftSize. The returned
 * vector holds the upper half of the magnitude spectrum (0 .. Nyquist),
 * normalized by 1/fftSize — the same math as DaqDisplayNode::spectrumSamples().
 *
 * @param samples input samples (typically normalized [-1, 1])
 * @param maxFftSize upper cap for the FFT size (default 4096)
 * @return half-spectrum magnitudes; empty if fewer than 2 samples usable
 */
inline QVector<float> magnitudeSpectrum(const QVector<float> &samples,
                                        int maxFftSize = 4096)
{
    const int n = samples.size();

    int fftSize = 1;
    while (fftSize * 2 <= n && fftSize * 2 <= maxFftSize)
        fftSize <<= 1;
    if (fftSize < 2)
        return {};

    // Hann window, then Cooley-Tukey radix-2 DIT (pattern: QDevioDisplayModelUi).
    const QVector<double> window = detail::hannWindow(fftSize);
    QVector<double> re(fftSize);
    QVector<double> im(fftSize, 0.0);
    for (int i = 0; i < fftSize; ++i)
        re[i] = samples.at(i) * window.at(i);

    // Bit reversal.
    for (int i = 1, j = 0; i < fftSize; ++i) {
        int bit = fftSize >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    // Radix-2 DIT butterflies.
    for (int len = 2; len <= fftSize; len <<= 1) {
        const double ang = 2.0 * M_PI / len;
        const double wRe = std::cos(ang);
        const double wIm = -std::sin(ang);
        for (int i = 0; i < fftSize; i += len) {
            double curRe = 1.0, curIm = 0.0;
            const int half = len / 2;
            for (int j = 0; j < half; ++j) {
                const double tRe = curRe * re[i + j + half] - curIm * im[i + j + half];
                const double tIm = curRe * im[i + j + half] + curIm * re[i + j + half];
                re[i + j + half] = re[i + j] - tRe;
                im[i + j + half] = im[i + j] - tIm;
                re[i + j] += tRe;
                im[i + j] += tIm;
                const double tmp = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = tmp;
            }
        }
    }

    // Magnitude spectrum, upper half (0 .. Nyquist).
    const int half = fftSize / 2;
    QVector<float> out;
    out.reserve(half);
    for (int i = 0; i < half; ++i)
        out.append(static_cast<float>(std::sqrt(re[i] * re[i] + im[i] * im[i])
                                      / double(fftSize)));
    return out;
}

} // namespace FftUtil
