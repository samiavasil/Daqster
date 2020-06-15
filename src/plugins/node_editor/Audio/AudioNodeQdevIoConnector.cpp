#include <XYSeriesIODevice.h>
#include <AudioNodeQdevIoConnector.h>
#include <AudioSourceDataModel.h>
#include <QDevIoDisplayModel.h>

#include<QDebug>

AudioNodeQdevIoConnector::AudioNodeQdevIoConnector(NodeDataModel *model ):
    NodeDataModelToQIODeviceConnector(model)
{
}

void AudioNodeQdevIoConnector::ConnectModels(QtNodes::NodeDataModel *dst_model)
{
    AudioSourceDataModel* model_src  = dynamic_cast<AudioSourceDataModel*>(m_src_model);
    QDevIoDisplayModel*   model_dst  = dynamic_cast<QDevIoDisplayModel*>(dst_model);

    if(model_src){
        std::shared_ptr<QIODevice>  xDevio = model_dst->device();

        if(xDevio) {
            QObject::connect(model_src, SIGNAL(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)),
                             model_dst, SLOT(ChangeAudioConnection(QAudioDeviceInfo, QAudioFormat)) );

            if( !xDevio->isOpen() ){
                xDevio->open(QIODevice::WriteOnly);
            }
        }
        model_src->IO_connect(xDevio);
    }
}

