#include "AudioNodeQdevIoConnector.h"
#include "AudioSourceDataModel.h"

AudioNodeQdevIoConnector::AudioNodeQdevIoConnector( AudioSourceDataModel* model, QObject *parent ):
    QObject(parent),
    m_model(model),
    m_Devio(nullptr)
{

}

void AudioNodeQdevIoConnector::SetDevIo(std::shared_ptr<QIODevice> dio)
{
    m_Devio = dio;
    if(m_model){
        m_model->IO_connect(m_Devio);
    }
}
#include<QDebug>
void AudioNodeQdevIoConnector::ModChanged()
{
    if(m_model){
        //m_model->;
        qDebug()<< "Model Changed";
    }
}
