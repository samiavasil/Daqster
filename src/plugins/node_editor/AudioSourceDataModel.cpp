#include "AudioSourceDataModel.h"
#include "AudioNodeQdevIoConnector.h"
#include "AudioSourceDataModel.h"
#include "AudioSourceDataModelUI.h"
#include <nodes/internal/Connection.hpp>
#include <QtMultimedia/QAudioInput>
#include<QDebug>

AudioSourceDataModel::AudioSourceDataModel()
{
    m_DevInfo = QAudioDeviceInfo::defaultInputDevice();
    m_FormatAudio = m_DevInfo.preferredFormat();

    m_connector = std::make_shared<AudioNodeQdevIoConnector>(this);
    m_Widget = new AudioSourceDataModelUI(m_DevInfo, m_FormatAudio);
    connect(m_Widget, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo&,QAudioFormat&)), this, SLOT(ChangeAudioConnection(QAudioDeviceInfo&,QAudioFormat&)));
    connect(m_Widget,SIGNAL(Start(AudioSourceDataModel::StartStop)),SLOT(StartAudio(AudioSourceDataModel::StartStop)));
    connect(this, SIGNAL(ModChanged()), m_connector.get(), SLOT(ModChanged()));
}

AudioSourceDataModel::~AudioSourceDataModel()
{

}

QJsonObject AudioSourceDataModel::save() const
{
    QJsonObject modelJson;

    modelJson["name"] = name();

    return modelJson;
}

unsigned int AudioSourceDataModel::nPorts(QtNodes::PortType portType) const
{
    int num = 0;
    switch (portType) {
    /*
    case QtNodes::PortType::In:
        num = 1;
        break;
    */
    case QtNodes::PortType::Out:
        num = 1;
        break;
    default:
        break;
    }
    return num;
}

QtNodes::NodeDataType AudioSourceDataModel::dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const
{
    return NodeDataType {"QDevIO", "IO"};
}

std::shared_ptr<QtNodes::NodeData> AudioSourceDataModel::outData(QtNodes::PortIndex port)
{
    return m_connector;
}

void AudioSourceDataModel::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex port)
{
    Q_ASSERT(0);
}

QWidget *AudioSourceDataModel::embeddedWidget()
{
    return m_Widget;
}

void AudioSourceDataModel::IO_connect(std::shared_ptr<QIODevice> io)
{
    m_devio = io;
    if( m_devio ){
        if( !m_devio->isOpen() ){
            m_devio->open(QIODevice::WriteOnly);
        }
        StartAudio(ASDM_START);
    }
}

void AudioSourceDataModel::ChangeAudioConnection(QAudioDeviceInfo &devInfo, QAudioFormat &formatAudio)
{
    if( devInfo.isFormatSupported(formatAudio) ){
        if(m_DevInfo != devInfo || m_FormatAudio != formatAudio){
            m_DevInfo = devInfo;
            m_FormatAudio = formatAudio;
            emit ModChanged();
            StartAudio(ASDM_RELOAD);
        }
        else{
            StartAudio(ASDM_START);
        }
    }
    else{
        qDebug() << "Unsupported Format... Stop Audio";
        StartAudio(ASDM_STOP);
    }
}


void AudioSourceDataModel::outputConnectionDeleted(const QtNodes::Connection &con)
{
    qDebug() << "Disconnected: " << con.getPortIndex(QtNodes::PortType::Out);
    StartAudio(ASDM_STOP);
}


void AudioSourceDataModel::destroyedObj(QObject* obj){
    qDebug() << "Destroyed: " << obj->objectName();
}

void AudioSourceDataModel::StartAudio(StartStop start){

        //if( m_DevInfo.isFormatSupported(m_FormatAudio) ) {
    switch (start) {
    case ASDM_STOP:{
        if(m_audio_src)
            m_audio_src->stop();
        break;
    }
    case ASDM_START:{
        if( !m_audio_src ){
            case ASDM_RELOAD:
            m_audio_src  = std::make_shared<QAudioInput>(m_DevInfo, m_FormatAudio);
            m_audio_src->setBufferSize(8000);
            m_audio_src->setObjectName(QString("AudioInput: %1").arg(m_DevInfo.deviceName()));
            connect(m_audio_src.get(),SIGNAL(destroyed(QObject*)), this,SLOT(destroyedObj(QObject*)));
            connect(m_audio_src.get(),SIGNAL(stateChanged(QAudio::State)), m_Widget, SLOT(AudioStateChanged(QAudio::State)) );
        }
        if(m_devio){
            m_audio_src->start(m_devio.get());
        }
        break;
    }

    default:
        break;
    }

}
