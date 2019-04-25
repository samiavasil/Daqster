#include "AudioNodeQdevIoConnector.h"
#include "AudioSourceDataModel.h"
#include "QDevIoDisplayModel.h"

#include<QDebug>

AudioNodeQdevIoConnector::AudioNodeQdevIoConnector(NodeDataModel *model ):
    NodeDataModelToQIODeviceConnector(model),
    m_model(nullptr),
    m_Devio(nullptr)
{
    m_model  = dynamic_cast<AudioSourceDataModel*>(model);
    if(m_model) {
        qDebug() << m_model;
    }
}

void AudioNodeQdevIoConnector::SetDevIo(std::shared_ptr<QIODevice> dio)
{
    m_Devio = dio;
    if(m_model){
        if( m_Devio ){
            if( !m_Devio->isOpen() ){
                m_Devio->open(QIODevice::WriteOnly);
            }
        }
        m_model->IO_connect(m_Devio);
    }
}

