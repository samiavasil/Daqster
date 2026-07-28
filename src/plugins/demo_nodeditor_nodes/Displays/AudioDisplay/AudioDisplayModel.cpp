#include "AudioDisplayModel.h"
#include <GenericQDevIoConnector.h>
#include <QDevioDisplayModelUi.h>
#include <XYSeriesIODevice.h>
#include <QDebug>
#include "LogCategories.h"

AudioDisplayModel::AudioDisplayModel()
    : QDevIoDisplayModel()
{
}

AudioDisplayModel::~AudioDisplayModel()
{
}

void AudioDisplayModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                  QtNodes::PortIndex const portIndex)
{
    // Let base class handle routing
    QDevIoDisplayModel::setInData(data, portIndex);

    // If we got a GenericQDevIoConnector with audio config, configure the view
    auto connector = std::dynamic_pointer_cast<GenericQDevIoConnector>(data);
    if (connector && connector->hasStreamConfig()) {
        auto config = connector->streamConfig();
        if (config.type == "audio") {
            configureAudioView(config.channels, config.sampleRate);
        }
    }
}

void AudioDisplayModel::configureAudioView(int channels, int sampleRate)
{
    Q_UNUSED(sampleRate);
    // The audio view (QDevioDisplayModelUi) is already set up by the base class.
    // For mixed streams, we'd extract the audio channel here.
    qCInfo(lcDemoNodes) << "AudioDisplayModel: configure for" << channels << "channels";
}
