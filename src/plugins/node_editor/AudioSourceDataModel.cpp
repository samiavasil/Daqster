#include "AudioSourceDataModel.h"
#include "AudioNodeQdevIoConnector.h"

AudioSourceDataModel::AudioSourceDataModel()
{
    m_connector = std::make_shared<AudioNodeQdevIoConnector>(this);
    m_FormatAudio.setChannelCount(1);
    m_FormatAudio.setCodec("audio/pcm");
    m_FormatAudio.setByteOrder(QAudioFormat::LittleEndian);
    m_FormatAudio.setSampleRate(8000);
    m_FormatAudio.setSampleSize(8);
    m_FormatAudio.setSampleType(QAudioFormat::UnSignedInt);
    m_DeviceInfo = QAudioDeviceInfo::defaultInputDevice();
    m_audio_src  = std::make_shared<QAudioInput>(m_DeviceInfo, m_FormatAudio);
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
    return NodeDataType { "AudioNodeQdevIoConnector",
                          "AudioNodeQdevIoConnector"};
}

std::shared_ptr<QtNodes::NodeData> AudioSourceDataModel::outData(QtNodes::PortIndex port)
{
    return m_connector;
}

void AudioSourceDataModel::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex port)
{

}

QWidget *AudioSourceDataModel::embeddedWidget()
{
    return nullptr;
}

void AudioSourceDataModel::IO_connect(std::shared_ptr<QIODevice> io)
{
    m_devio = io;
    if( m_devio ){
        m_devio->open(QIODevice::WriteOnly);
        m_audio_src->start(m_devio.get());
    }
}
