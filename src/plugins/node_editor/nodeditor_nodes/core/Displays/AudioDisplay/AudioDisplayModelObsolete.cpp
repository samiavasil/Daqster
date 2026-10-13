#include "AudioDisplayModelObsolete.h"
#include <GenericQDevIoConnectorObsolete.h>
#include <QDebug>
#include "LogCategories.h"

AudioDisplayModelObsolete::AudioDisplayModelObsolete()
    : QDevIoDisplayModelObsolete()
{
}

AudioDisplayModelObsolete::~AudioDisplayModelObsolete()
{
}

void AudioDisplayModelObsolete::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                          QtNodes::PortIndex const portIndex)
{
    // Let base class handle routing
    QDevIoDisplayModelObsolete::setInData(data, portIndex);

    // If we got a GenericQDevIoConnectorObsolete with audio config, configure the view
    auto connector = std::dynamic_pointer_cast<GenericQDevIoConnectorObsolete>(data);
    if (connector && connector->hasStreamConfig()) {
        auto config = connector->streamConfig();
        if (config.type == "audio") {
            configureAudioView(config.channels, config.sampleRate);
        }
    }
}

void AudioDisplayModelObsolete::configureAudioView(int channels, int sampleRate)
{
    // Model-only since REQ-SW-PL-056: the waveform view lives in the GUI
    // plugin (QDevioDisplayModelUiObsolete, created by NodeWidgetFactory's
    // "AudioDisplayObsolete" creator) and reacts to this signal — it sets its
    // series count to the stream's channel count.
    Q_EMIT audioViewConfigChanged(qMax(1, channels), sampleRate);
    qCInfo(lcDemoNodes) << "AudioDisplayModelObsolete: configured for" << channels
                        << "channels at" << sampleRate << "Hz";
}
