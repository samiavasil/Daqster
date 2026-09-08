#include <AudioWorkerObsolete.h>
#include <QDebug>
#include "LogCategories.h"

AudioWorkerObsolete::AudioWorkerObsolete(std::shared_ptr<QIODevice> devio, QObject *parent):QObject(parent),
    m_devio(devio)
{
    m_devio = devio;
    m_audio_src = nullptr;

}

AudioWorkerObsolete::~AudioWorkerObsolete(){
    m_audio_src->stop();
}

void AudioWorkerObsolete::DoWork() {
    QString result;
    QAudioDeviceInfo m_DevInfo = AudioCompat::defaultInputDevice();
    QAudioFormat  m_FormatAudio = AudioCompat::preferredFormat(m_DevInfo);

    UpdateAudioDevice(m_DevInfo, m_FormatAudio);
    emit resultReady(result);
}

void AudioWorkerObsolete::Start(AudioSourceDataModelUI::StartStop status)
{
    switch (status) {
    case AudioSourceDataModelUI::ASDM_STOP:{
        m_audio_src->stop();
        break;
    }
    case AudioSourceDataModelUI::ASDM_START:
    case AudioSourceDataModelUI::ASDM_RELOAD:{
        m_audio_src->start(m_devio.get());
        if(QAudio::StoppedState == m_audio_src->state()) {

            emit m_audio_src->stateChanged(QAudio::StoppedState);
        }
        break;
    }
    }
}

void AudioWorkerObsolete::UpdateAudioDevice(QAudioDeviceInfo devInfo, QAudioFormat formatAudio)
{
    bool was_started = false;

    if(m_audio_src != nullptr) {
        if(QAudio::StoppedState != m_audio_src->state()) {
            m_audio_src->stop();
            disconnect(m_audio_src.get());
            was_started = true;
        }
    }
    m_audio_src  = std::make_shared<AudioCompat::AudioInput>(devInfo, formatAudio);
    m_audio_src->setBufferSize(1000);
    qCDebug(lcDemoNodes) << m_audio_src->bufferSize();
    m_audio_src->setObjectName(QString("AudioInput: %1").arg(AudioCompat::deviceName(devInfo)));
    connect(m_audio_src.get(),SIGNAL(stateChanged(QAudio::State)), this, SIGNAL(stateChanged(QAudio::State)) );
    if(was_started) {
        Start(AudioSourceDataModelUI::ASDM_RELOAD);
    }
    emit ChangeAudioConnection(devInfo, formatAudio);
}
