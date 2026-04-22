#ifndef AUDIOFRAMEDECODER_H
#define AUDIOFRAMEDECODER_H

#include <QtGlobal>
#include <QtMultimedia/QAudioFormat>

/**
 * Decode a single PCM sample from raw bytes and return a normalized value in [-1, 1].
 * The decoder is configured once per audio format and used in the hot path without
 * format branching.
 */
class AudioFrameDecoder
{
public:
    AudioFrameDecoder();

    bool configure(const QAudioFormat &format);

    int bytesPerSample() const { return m_bytesPerSample; }
    int channels() const { return m_channels; }
    int frameBytes() const { return m_bytesPerSample * m_channels; }

    qreal decodeNormalizedSample(const char *samplePtr) const;
    bool isValid() const { return m_decoder != nullptr; }

private:
    typedef qreal (*DecoderFn)(const char *);

    DecoderFn m_decoder = nullptr;
    int m_bytesPerSample = 2;
    int m_channels = 1;
};

#endif // AUDIOFRAMEDECODER_H