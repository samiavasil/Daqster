#include <AudioSourceDataModel.h>
#include <AudioNodeQdevIoConnector.h>
#include <AudioSourceDataModel.h>
#include <AudioSourceDataModelUI.h>
#include <nodes/internal/Connection.hpp>
#include <QtMultimedia/QAudioInput>
#include <EventThreadPull.h>
#include <AudioWorker.h>
#include <QDebug>

AudioSourceDataModel::AudioSourceDataModel()
{
    qRegisterMetaType<AudioSourceDataModel::StartStop>("AudioSourceDataModel::StartStop");
    
    QAudioDeviceInfo devInfo = QAudioDeviceInfo::defaultInputDevice();
    QAudioFormat formatAudio = devInfo.preferredFormat();
    
    m_connector = std::make_shared<AudioNodeQdevIoConnector>(this);
    m_Widget = new AudioSourceDataModelUI(devInfo, formatAudio);
    m_Widget->setWindowFlags(Qt::Dialog);
    m_Widget->setWindowModality(Qt::WindowModal);
    connect(m_Widget,SIGNAL(Start(AudioSourceDataModel::StartStop)),SIGNAL(StartAudio(AudioSourceDataModel::StartStop)));
    connect(m_Widget,SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)),
            this, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)));
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
    unsigned int num = 0;
    
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
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

QWidget *AudioSourceDataModel::embeddedWidget()
{
    return m_Widget;
}

void AudioSourceDataModel::IO_connect(std::shared_ptr<QIODevice> io)
{
    
    if(io != nullptr){
        AudioWorker* worker= new AudioWorker(io);
        connect(this, SIGNAL(destroyed()), worker, SLOT(deleteLater()));
        connect(this, SIGNAL(StartAudio(AudioSourceDataModel::StartStop)),
                worker, SLOT(Start(AudioSourceDataModel::StartStop)));
        connect(worker, SIGNAL(stateChanged(QAudio::State)),
                m_Widget, SLOT(AudioStateChanged(QAudio::State)) );
        connect(this, SIGNAL(disconnected()), worker, SLOT(deleteLater()));
        connect(this, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)),
                worker, SLOT(UpdateAudioDevice(QAudioDeviceInfo, QAudioFormat)));
        
        EventThreadPull::thread_pull().AddWorker(worker);
    }
    
    emit StartAudio(ASDM_START);
}

void AudioSourceDataModel::outputConnectionDeleted(const QtNodes::Connection &con)
{
    qDebug() << "Disconnected: " << con.getPortIndex(QtNodes::PortType::Out);
    emit disconnected();
}


void AudioSourceDataModel::destroyedObj(QObject* obj){
    qDebug() << "Destroyed: " << obj->objectName();
}

