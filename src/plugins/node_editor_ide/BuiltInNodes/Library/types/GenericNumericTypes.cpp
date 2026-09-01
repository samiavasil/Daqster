#include "GenericNumericTypes.h"

void MultiplexedBuffer::decodeToNormalized(
    QVector<QVector<double>>& outChannels) const
{
    const int nChannels = config.totalChannels();
    if (nChannels == 0 || data.isEmpty()) return;

    int frameBytes = config.bytesPerFrame();
    if (frameBytes == 0) return;

    int totalFrames = data.size() / frameBytes;

    outChannels.resize(nChannels);
    for (int ch = 0; ch < nChannels; ++ch)
        outChannels[ch].resize(totalFrames);

    const char* ptr = data.constData();
    for (int frame = 0; frame < totalFrames; ++frame) {
        for (int ch = 0; ch < nChannels; ++ch) {
            const auto& desc = config.channels[ch];
            int byteSize = sampleTypeByteSize(desc.sampleType);

            // Unified SampledDecoder convention (REQ-SW-PL-022 AC 3):
            // signed/unsigned ÷ (2^(bits-1) − 1), clamp to [-1, 1]; floats clamped.
            outChannels[ch][frame] =
                SampledDecoder::decodeNormalizedSample(ptr, desc.sampleType,
                                                       SampleEndian::LittleEndian);
            ptr += byteSize;
        }
    }
}