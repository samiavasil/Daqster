#include "AudioNodeQdevIoConnector.h"
#include "AudioSourceDataModel.h"
#include "QDevIoDisplayModel.h"
#include<QDebug>

AudioNodeQdevIoConnector::AudioNodeQdevIoConnector( AudioSourceDataModel* model, QObject *parent ):
    QObject(parent),
    m_model(model),
    m_Devio(nullptr)
{

}

void AudioNodeQdevIoConnector::SetDevIo(QSharedPointer<QIODevice> dio)
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

void AudioNodeQdevIoConnector::ConnectPair(std::shared_ptr<QDevIoDisplayModel> display_model)
{

}

void AudioNodeQdevIoConnector::ModChanged()
{
    if(m_model){
        QDevIoDisplayModel* model = dynamic_cast<QDevIoDisplayModel*>(m_model);
        if( model ){
            model->UpdateModel(10);
        }
        //m_model->pause;
        //m_model->update;

        //m_model->continue;
        qDebug()<< "Model Changed";
    }
}
