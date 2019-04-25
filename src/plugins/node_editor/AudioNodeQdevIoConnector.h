#ifndef AUDIONODEQDEVIOCONNECTOR_H
#define AUDIONODEQDEVIOCONNECTOR_H

#include <memory>
#include <NodeDataModelToQIODeviceConnector.h>
#include <QSharedPointer>

#include<nodes/NodeDataModel>

using QtNodes::NodeDataModel;

class QIODevice;
class AudioSourceDataModel;
class QDevIoDisplayModel;

class AudioNodeQdevIoConnector : public NodeDataModelToQIODeviceConnector
{
    Q_OBJECT
public:

    explicit AudioNodeQdevIoConnector( NodeDataModel* model);

    virtual void SetDevIo(std::shared_ptr<QIODevice> dio);

    virtual NodeDataType type() const
    {
        return NodeDataType { "AudioNodeQdevIoConnector",
            "AudioNodeQdevIoConnector"};
    }

    std::shared_ptr<QIODevice>& DevIo(){ return m_Devio;}

private:
    AudioSourceDataModel* m_model;
    std::shared_ptr<QIODevice> m_Devio;

};

#endif // AUDIONODEQDEVIOCONNECTOR_H
