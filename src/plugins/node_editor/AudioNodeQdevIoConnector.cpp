#include "AudioNodeQdevIoConnector.h"

AudioNodeQdevIoConnector::AudioNodeQdevIoConnector()
{

}

void AudioNodeQdevIoConnector::SetDevIo(std::shared_ptr<QIODevice> dio)
{
    m_Devio = dio;
}
