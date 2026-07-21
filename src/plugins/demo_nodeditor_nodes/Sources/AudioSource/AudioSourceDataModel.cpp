#include <AudioSourceDataModel.h>
#include <AudioNodeQdevIoConnector.h>
#include <AudioSourceDataModel.h>
#include <AudioSourceDataModelUI.h>
#include <EventThreadPull.h>
#include <AudioWorker.h>
#include <QDebug>

using QtNodes::NodeDataType;

AudioSourceDataModel::AudioSourceDataModel()
{
    qRegisterMetaType<AudioSourceDataModel::StartStop>("AudioSourceDataModel::StartStop");
    qRegisterMetaType<std::shared_ptr<QIODevice>>("std::shared_ptr<QIODevice>");
    
    m_DevInfo = AudioCompat::defaultInputDevice();
    m_FormatAudio = AudioCompat::preferredFormat(m_DevInfo);
    
    m_connector = std::make_shared<AudioNodeQdevIoConnector>(this);
    m_Widget = new AudioSourceDataModelUI(&m_DevInfo, &m_FormatAudio);
    m_Widget->setWindowFlags(Qt::Window
                             | Qt::WindowTitleHint
                             | Qt::WindowSystemMenuHint
                             | Qt::WindowMinMaxButtonsHint
                             | Qt::WindowCloseButtonHint);
    m_Widget->setWindowModality(Qt::NonModal);
    connect(m_Widget,SIGNAL(Start(AudioSourceDataModel::StartStop)),SIGNAL(StartAudio(AudioSourceDataModel::StartStop)));
}

AudioSourceDataModel::~AudioSourceDataModel()
{
    // Widget lifetime is owned by the node/view framework.
    // Explicit delete here causes double-free during scene teardown.
    m_Widget = nullptr;
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

std::shared_ptr<QtNodes::NodeData> AudioSourceDataModel::outData(QtNodes::PortIndex const port)
{
    return m_connector;
}

void AudioSourceDataModel::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const port)
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
        /*UI Widget change Audio type*/
        connect(m_Widget, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)),
                worker, SLOT(UpdateAudioDevice(QAudioDeviceInfo, QAudioFormat)));
        /*When the audio worker update Audio type the model notify for change */
        connect(worker,SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)),
                this, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)));
        EventThreadPull::instance().AddWorker(worker);
    }
    
    emit StartAudio(ASDM_START);
}

void AudioSourceDataModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    qDebug() << "Disconnected port:" << conId.outPortIndex;
    emit disconnected();
}


void AudioSourceDataModel::destroyedObj(QObject* obj){
    qDebug() << "Destroyed: " << obj->objectName();
}
