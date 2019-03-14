#include "AudioSourceDataModel.h"
#include "AudioNodeQdevIoConnector.h"
#include "AudioSourceDataModel.h"
#include "AudioSourceDataModelUI.h"
#include <QtMultimedia/QAudioInput>
#include<QDebug>

AudioSourceDataModel::AudioSourceDataModel()
{
    m_connector = std::make_shared<AudioNodeQdevIoConnector>(this);
    m_Widget = new AudioSourceDataModelUI;
    connect(m_Widget, SIGNAL(ReloadAudioConnection()), this, SLOT(ReloadAudioConnection()));
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
        if(m_audio_src)
            m_audio_src->stop();
        QAudioFormat format = m_Widget->FormatAudio();
        QAudioDeviceInfo dInfo = m_Widget->DevInfo();
        if( dInfo.isFormatSupported(format) ) {
            m_audio_src  = std::make_shared<QAudioInput>(dInfo, format);
            m_audio_src->setObjectName(QString("AudioInput: %1").arg(dInfo.deviceName()));
            connect(m_audio_src.get(),SIGNAL(destroyed(QObject*)), this,SLOT(destroyedObj(QObject*)));
            if( !m_devio->isOpen() ){
                m_devio->open(QIODevice::WriteOnly);
            }
            m_audio_src->start(m_devio.get());
            qDebug() << __FUNCTION__ << "Run DevName:" << dInfo.deviceName() << format;
        }
        else{
            qDebug() << __FUNCTION__ << "DevName:" << dInfo.deviceName() << "Not Supported Format" << format;
        }
    }
    else{
        if(m_audio_src)
            m_audio_src->stop();
    }
}

void AudioSourceDataModel::ReloadAudioConnection()
{
    IO_connect(m_devio);
}

void AudioSourceDataModel::destroyedObj(QObject* obj){
    qDebug() << "Destroyed: " << obj->objectName();
}
