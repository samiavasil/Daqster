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
    // Real view configuration — replaces the old no-op stub (REQ-SW-PL-022
    // AC 6). The base QDevioDisplayModelUi renders the waveform; here we make
    // sure the series count matches the stream and the audio view is current.
    auto *displayUi = dynamic_cast<QDevioDisplayModelUi *>(m_stack->widget(m_audioViewIndex));
    if (displayUi == nullptr)
        return;

    displayUi->SetSeries(0, qMax(1, channels));
    m_stack->setCurrentIndex(m_audioViewIndex);
    qCInfo(lcDemoNodes) << "AudioDisplayModel: configured for" << channels
                        << "channels at" << sampleRate << "Hz";
}
