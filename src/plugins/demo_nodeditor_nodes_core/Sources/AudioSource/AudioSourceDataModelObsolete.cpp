#include <AudioSourceDataModelObsolete.h>
#include <AudioNodeQdevIoConnectorObsolete.h>
#include <EventThreadPullObsolete.h>
#include <AudioWorkerObsolete.h>
#include <QDebug>
#include "LogCategories.h"

using QtNodes::NodeDataType;

AudioSourceDataModelObsolete::AudioSourceDataModelObsolete()
{
    qRegisterMetaType<AudioStartStop>("AudioStartStop");
    qRegisterMetaType<std::shared_ptr<QIODevice>>("std::shared_ptr<QIODevice>");

    m_DevInfo = AudioCompat::defaultInputDevice();
    m_FormatAudio = AudioCompat::preferredFormat(m_DevInfo);

    m_connector = std::make_shared<AudioNodeQdevIoConnectorObsolete>(this);
}

AudioSourceDataModelObsolete::~AudioSourceDataModelObsolete() = default;

QJsonObject AudioSourceDataModelObsolete::save() const
{
    QJsonObject modelJson = NodeDelegateModel::save();
    return modelJson;
}

unsigned int AudioSourceDataModelObsolete::nPorts(QtNodes::PortType portType) const
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

QtNodes::NodeDataType AudioSourceDataModelObsolete::dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const
{
    return NodeDataType {"QDevIO", "IO"};
}

std::shared_ptr<QtNodes::NodeData> AudioSourceDataModelObsolete::outData(QtNodes::PortIndex const port)
{
    return m_connector;
}

void AudioSourceDataModelObsolete::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

void AudioSourceDataModelObsolete::IO_connect(std::shared_ptr<QIODevice> io)
{

    if(io != nullptr){
        AudioWorkerObsolete* worker= new AudioWorkerObsolete(io);
        connect(this, SIGNAL(destroyed()), worker, SLOT(deleteLater()));
        connect(this, SIGNAL(StartAudio(AudioStartStop)),
                worker, SLOT(Start(AudioStartStop)));
        /* Worker state is forwarded as a model signal; the GUI plugin's
           NodeWidgetFactory routes it to AudioSourceDataModelUI. */
        connect(worker, SIGNAL(stateChanged(QAudio::State)),
                this, SIGNAL(audioStateChanged(QAudio::State)));
        connect(this, SIGNAL(disconnected()), worker, SLOT(deleteLater()));
        /*UI Widget change Audio type - the factory connects the widget signal
          to this model signal, which then reaches the worker. */
        connect(this, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)),
                worker, SLOT(UpdateAudioDevice(QAudioDeviceInfo, QAudioFormat)));
        /*When the audio worker update Audio type the model notify for change */
        connect(worker,SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)),
                this, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)));
        EventThreadPullObsolete::instance().AddWorker(worker);
    }

    emit StartAudio(ASDM_START);
}

void AudioSourceDataModelObsolete::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    qCInfo(lcDemoNodes) << "Disconnected port:" << conId.outPortIndex;
    emit disconnected();
}


void AudioSourceDataModelObsolete::destroyedObj(QObject* obj){
    qCDebug(lcDemoNodes) << "Destroyed: " << obj->objectName();
}
