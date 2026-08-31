#include "AudioDisplayModelObsolete.h"
#include <GenericQDevIoConnectorObsolete.h>
#include <QDevioDisplayModelUiObsolete.h>
#include <XYSeriesIODeviceObsolete.h>
#include <QDebug>
#include <QStackedWidget>
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
    // Real view configuration — replaces the old no-op stub (REQ-SW-PL-022
    // AC 6). The base QDevioDisplayModelUiObsolete renders the waveform; here
    // we make sure the series count matches the stream and the audio view is
    // current.
    auto *displayUi = dynamic_cast<QDevioDisplayModelUiObsolete *>(m_stack->widget(m_audioViewIndex));
    if (displayUi == nullptr)
        return;

    displayUi->SetSeries(0, qMax(1, channels));
    m_stack->setCurrentIndex(m_audioViewIndex);
    qCInfo(lcDemoNodes) << "AudioDisplayModelObsolete: configured for" << channels
                        << "channels at" << sampleRate << "Hz";
}
