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
    if(0) {//FIX ME
        m_audio_src  = std::make_shared<QAudioInput>(m_FormatAudio);
        m_audio_src->setObjectName(QString("AudioInput: %1").arg(m_DevInfo.deviceName()));
        //      connect(m_audio_src.get(),SIGNAL(destroyed(QObject*)), this,SLOT(destroyedObj(QObject*)));
        connect(m_audio_src.get(),SIGNAL(stateChanged(QAudio::State)), this, SIGNAL(stateChanged(QAudio::State)) );
        //m_audio_src->start(m_devio.get());
    }
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
        break;
    }
    }
}

void AudioWorker::UpdateAudioDevice(QAudioDeviceInfo devInfo, QAudioFormat formatAudio)
{
#if 0
    if( devInfo.isFormatSupported(formatAudio) ){
        if(m_DevInfo != devInfo || m_FormatAudio != formatAudio){ //DELL ME - remove m_DevInfo m_FormatAudio
            m_DevInfo = devInfo;
            m_FormatAudio = formatAudio;
            emit StartAudio(ASDM_RELOAD);
        }
        else{
            emit StartAudio(ASDM_START);
        }
    }
    else{
        qDebug() << "Unsupported Format... Stop Audio";
        emit StartAudio(ASDM_STOP);
    }
#endif
    bool was_started = false;
    if(m_audio_src != nullptr) {
        if(QAudio::StoppedState != m_audio_src->state()) {
            m_audio_src->stop();
            disconnect(m_audio_src.get());
            was_started = true;
        }
    }
    m_audio_src  = std::make_shared<QAudioInput>(devInfo, formatAudio);
    //      m_audio_src->setNotifyInterval(40);
    m_audio_src->setBufferSize(1000);
    qDebug() << m_audio_src->bufferSize();
    m_audio_src->setObjectName(QString("AudioInput: %1").arg(devInfo.deviceName()));
    connect(m_audio_src.get(),SIGNAL(stateChanged(QAudio::State)), this, SIGNAL(stateChanged(QAudio::State)) );
    if(was_started) {
        m_audio_src->start(m_devio.get());
    }
}
