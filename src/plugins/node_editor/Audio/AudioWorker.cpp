#include <AudioWorker.h>
#include <QDebug>

AudioWorker::AudioWorker(std::shared_ptr<QIODevice> devio, QObject *parent):InEventLoopWorker(parent),
    m_devio(devio)
{
    m_devio = devio;
    m_audio_src = nullptr;

}

AudioWorker::~AudioWorker(){
    m_audio_src->stop();
}

void AudioWorker::DoWork() {
    QString result;
    disconnect(sender() ,SIGNAL(operate()), this, SLOT(DoWork()));
    QAudioDeviceInfo m_DevInfo = QAudioDeviceInfo::defaultInputDevice();
    QAudioFormat  m_FormatAudio = m_DevInfo.preferredFormat();

    UpdateAudioDevice(m_DevInfo, m_FormatAudio);
    emit resultReady(result);
}

void AudioWorker::Start(AudioSourceDataModel::StartStop status)
{
    switch (status) {
    case AudioSourceDataModel::ASDM_STOP:{
        m_audio_src->stop();
        break;
    }
    case AudioSourceDataModel::ASDM_START:
    case AudioSourceDataModel::ASDM_RELOAD:{
        m_audio_src->start(m_devio.get());
        if(QAudio::StoppedState == m_audio_src->state()) {

            emit m_audio_src->stateChanged(QAudio::StoppedState);
        }
        break;
    }
    }
}

void AudioWorker::UpdateAudioDevice(QAudioDeviceInfo devInfo, QAudioFormat formatAudio)
{
    bool was_started = false;

    if(m_audio_src != nullptr) {
        if(QAudio::StoppedState != m_audio_src->state()) {
            m_audio_src->stop();
            disconnect(m_audio_src.get());
            was_started = true;
        }
    }
    m_audio_src  = std::make_shared<QAudioInput>(devInfo, formatAudio);
    m_audio_src->setBufferSize(1000);
    qDebug() << m_audio_src->bufferSize();
    m_audio_src->setObjectName(QString("AudioInput: %1").arg(devInfo.deviceName()));
    connect(m_audio_src.get(),SIGNAL(stateChanged(QAudio::State)), this, SIGNAL(stateChanged(QAudio::State)) );
    if(was_started) {
        Start(AudioSourceDataModel::ASDM_RELOAD);
    }
    emit ChangeAudioConnection(devInfo, formatAudio);
}
