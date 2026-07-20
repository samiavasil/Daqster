#include "GenericNumericTypes.h"
#include <cstring>

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

            double value = 0.0;
            switch (desc.sampleType) {
                case SampleType::INT16: {
                    int16_t raw;
                    std::memcpy(&raw, ptr, 2);
                    value = raw / 32768.0;
                    break;
                }
                case SampleType::UINT16: {
                    uint16_t raw;
                    std::memcpy(&raw, ptr, 2);
                    value = (raw - 32768.0) / 32768.0;
                    break;
                }
                case SampleType::INT32: {
                    int32_t raw;
                    std::memcpy(&raw, ptr, 4);
                    value = raw / 2147483648.0;
                    break;
                }
                case SampleType::UINT32: {
                    uint32_t raw;
                    std::memcpy(&raw, ptr, 4);
                    value = (raw - 2147483648.0) / 2147483648.0;
                    break;
                }
                case SampleType::FLOAT32: {
                    float raw;
                    std::memcpy(&raw, ptr, 4);
                    value = static_cast<double>(raw);
                    break;
                }
                case SampleType::FLOAT64: {
                    double raw;
                    std::memcpy(&raw, ptr, 8);
                    value = raw;
                    break;
                }
            }

            outChannels[ch][frame] = value;
            ptr += byteSize;
        }
    }
}
