#ifndef AUDIONODEQDEVIOCONNECTOR_H
#define AUDIONODEQDEVIOCONNECTOR_H

#include <memory>
#include <NodeDataModelToQIODeviceConnector.h>
#include <QSharedPointer>

#include<nodes/NodeDataModel>

using QtNodes::NodeDataModel;

class QIODevice;

class AudioNodeQdevIoConnector : public NodeDataModelToQIODeviceConnector
{

public:

    explicit AudioNodeQdevIoConnector( NodeDataModel* model);

    virtual NodeDataType type() const //TODO : TBD ???
    {
        return NodeDataType { "AudioNodeQdevIoConnector",
            "AudioNodeQdevIoConnector"};
    }
    virtual void ConnectModels(NodeDataModel* dst_model);
};

#endif // AUDIONODEQDEVIOCONNECTOR_H
